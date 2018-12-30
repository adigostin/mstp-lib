
#include "ethernet.h"
#include "clock.h"
#include <TM4C1294KCPDT.h>
#include <assert.h>
#include <driverlib/emac.h>
#include <string.h>

struct dma_descriptor_t
{
    volatile uint32_t ui32CtrlStatus;
    volatile uint32_t ui32Count;
    void *pvBuffer1;
	union
	{
		dma_descriptor_t* pLink;
		void* pvBuffer2;
	} DES3;
};

static constexpr size_t tx_desc_count = 3;
static constexpr size_t rx_desc_count = 20;

static dma_descriptor_t rx_descriptors[rx_desc_count];
static dma_descriptor_t tx_descriptors[tx_desc_count];

//static uint32_t muRxDescIndex;
static size_t tx_desc_write_index;
static size_t rx_desc_read_index;

static constexpr size_t rx_buffer_size = 1500;
static constexpr size_t tx_buffer_size = 1500;

static uint8_t rx_buffers[rx_desc_count][rx_buffer_size];
static uint8_t tx_buffer[tx_buffer_size];

extern "C" void EMAC0_IRQHandler()
{
	assert(false);
}

void ethernet_get_received (ethernet_rx_callback rx_callback)
{
	while (true)
	{
		auto desc = &rx_descriptors[rx_desc_read_index];
		if (desc->ui32CtrlStatus & (1u << 31))
			break;

		const uint8_t* data = rx_buffers[rx_desc_read_index];
		size_t size = (desc->ui32CtrlStatus & 0x3FFF0000) >> 16;
		rx_callback (data, size);
	
		desc->ui32CtrlStatus |= (1u << 31);

		rx_desc_read_index++;
		if (rx_desc_read_index == rx_desc_count)
			rx_desc_read_index = 0;
	}

	// In case we ran out of descriptors and the RX HW was suspended, wake it up.
	EMAC0->RXPOLLD = 0;
}
/*
void ethernet_transmit (const void *data, size_t size)
{
	assert (size <= sizeof(tx_buffer));

	memcpy (tx_buffer, data, size);

	assert((tx_descriptors[tx_desc_write_index].ui32CtrlStatus & DES0_TX_CTRL_OWN) == 0);

	// Fill in the packet size and tell the transmitter to start work.
	tx_descriptors[tx_desc_write_index].ui32Count = (uint32_t)size;
	tx_descriptors[tx_desc_write_index].ui32CtrlStatus =
	    (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG |
	        DES0_TX_CTRL_INTERRUPT | DES0_TX_CTRL_IP_ALL_CKHSUMS |
	        DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_OWN);

	tx_desc_write_index++;
	if (tx_desc_write_index == tx_desc_count)
		tx_desc_write_index = 0;

	// Tell the DMA to reacquire the descriptor now that we've filled it in.
	EMAC0->TXPOLLD = 0;
}
*/
void ethernet_init (const uint8_t *mac_address)
{
	// Enable the Ethernet modules.
	SYSCTL->RCGCEMAC |= 1;
	SYSCTL->RCGCEPHY |= 1;

	// Wait until the MAC peripheral is ready.
	while ((SYSCTL->PREMAC & 1) == 0)
		;

	EMAC0->PC = (1 << 10) // MDIX Enable
		| (1 << 3) // Auto Negotiation Enable
		| (3 << 1); // ANMODE = 3 (100Base-TX, Full-Duplex)
	
	// Reset the PHY peripheral.
	SYSCTL->SREPHY = 1;
	for (volatile int i = 0; i < 16; i++);
	SYSCTL->SREPHY = 0;
	while ((SYSCTL->PREPHY & 1) == 0);
    // Reset the MAC regardless of whether the PHY connection changed or not.
	EMAC0->DMABUSMOD |= 1;
	while (EMAC0->DMABUSMOD & 1);

    // Make sure that the DMA software reset is clear before continuing.
    while (EMAC0->DMABUSMOD & 1);

	EMAC0->DMABUSMOD = (1 << 26) // MB
		| (4 << 8) // Programmable Burst Length
		| (1 << 1); // DMA Arbitration Scheme (Fixed Prio)

	// The frequency of the System Clock is 100 to 150 MHz providing a MDIO clock of SYSCLK/62.
	EMAC0->MIIADDR = EMAC0->MIIADDR & ~(15 << 2) | (1 << 2);

    // Disable all the MMC interrupts as these are enabled by default at reset.
	EMAC0->MMCRXIM = 0xFFFFFFFF;
    EMAC0->MMCTXIM = 0xFFFFFFFF;

	// Set MAC configuration options.
	EMACConfigSet(EMAC0_BASE,
	    (EMAC_CONFIG_FULL_DUPLEX | EMAC_CONFIG_CHECKSUM_OFFLOAD |
	        EMAC_CONFIG_7BYTE_PREAMBLE | EMAC_CONFIG_IF_GAP_96BITS |
	        EMAC_CONFIG_USE_MACADDR0 |
	        EMAC_CONFIG_SA_FROM_DESCRIPTOR |
	        EMAC_CONFIG_BO_LIMIT_1024),
	    (EMAC_MODE_RX_STORE_FORWARD |
	        EMAC_MODE_TX_STORE_FORWARD |
	        EMAC_MODE_TX_THRESHOLD_64_BYTES |
	        EMAC_MODE_RX_THRESHOLD_64_BYTES),
	    0);

	// ----------------------------------------------------------
	// Initialize descriptors.

	for (size_t i = 0; i < tx_desc_count; i++)
	{
		tx_descriptors[i].ui32CtrlStatus = (1 << 29) // LS: Last Segment
			| (1 << 28) // FS: First Segment
			| (1 << 30) // IC: Interrupt on Completion
			| (1 << 20) // TCH: Second Address Chained
			| (3 << 22); // CIC: Checksum Insertion Control - 0x3 = Insert a TCP/UDP/ICMP checksum that is fully calculated in this engine
		tx_descriptors[i].ui32Count = sizeof(tx_buffer);
		tx_descriptors[i].pvBuffer1 = tx_buffer;
		tx_descriptors[i].DES3.pLink = &tx_descriptors[(i + 1) % tx_desc_count];
	}

	for (size_t i = 0; i < rx_desc_count; i++)
	{
		rx_descriptors[i].ui32CtrlStatus = (1u << 31);
		rx_descriptors[i].ui32Count = (1 << 14) | rx_buffer_size;
		rx_descriptors[i].pvBuffer1 = &rx_buffers[i];
		rx_descriptors[i].DES3.pLink = &rx_descriptors[(i + 1) % rx_desc_count];
	}

    EMAC0->RXDLADDR = (uint32_t)rx_descriptors;
	EMAC0->TXDLADDR = (uint32_t)tx_descriptors;

	tx_desc_write_index = 0;
	rx_desc_read_index = 0;

	// ----------------------------------------------------------

	// Program the hardware with its MAC address (for filtering).
	EMACAddrSet(EMAC0_BASE, 0, mac_address);

	// Set MAC filtering options.  We receive all broadcast and multicast
	// packets along with those addressed specifically for us.
	EMACFrameFilterSet(EMAC0_BASE, (EMAC_FRMFILTER_SADDR |
	                                   EMAC_FRMFILTER_PASS_MULTICAST |
	                                   EMAC_FRMFILTER_PASS_NO_CTRL));

	// Clear any pending interrupts.
	EMACIntClear(EMAC0_BASE, EMACIntStatus(EMAC0_BASE, false));
/*
    // Configure Ethernet LEDs.
	uint8_t ui8PHYAddr = 0;
    EMACPHYWrite(EMAC0_BASE, ui8PHYAddr, EPHY_LEDCFG, 0x223);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    GPIOPinConfigure(GPIO_PK4_EN0LED0);
    GPIOPinConfigure(GPIO_PK5_EN0LED2);
    GPIOPinTypeEthernetLED(GPIO_PORTK_BASE, GPIO_PIN_4);
    GPIOPinTypeEthernetLED(GPIO_PORTK_BASE, GPIO_PIN_5);
*/
	// Mark all receive descriptors as available to the DMA to start RX processing.
	for (uint32_t i = 0; i < rx_desc_count; i++)
		rx_descriptors[i].ui32CtrlStatus = DES0_RX_CTRL_OWN;

	// Enable the Ethernet MAC transmitter and receiver.
	EMAC0->DMAOPMODE |= (1 << 13) | (1 << 1); // set ST and SR
	EMAC0->CFG |= (1 << 3) | (1 << 2);        // set TE and RE

	//NVIC_EnableIRQ(EMAC0_IRQn);
	//EMAC0->DMAIM |= (1 << 6) | (1 << 16); // set RIE (Receive Interrupt Enable) and NIE
}