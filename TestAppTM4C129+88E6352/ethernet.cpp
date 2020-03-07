
#include "ethernet.h"
#include "clock.h"
#include <TM4C1294KCPDT.h>
#include <assert.h>
#include <string.h>

struct dma_descriptor_t
{
    volatile uint32_t status;
    volatile uint32_t count;
    void* buffer1;
	union
	{
		dma_descriptor_t* next;
		void* buffer2;
	};
};

static constexpr size_t tx_desc_count = 3;
static constexpr size_t rx_desc_count = 20;

static dma_descriptor_t rx_descriptors[rx_desc_count];
static dma_descriptor_t tx_descriptors[tx_desc_count];

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
	__DSB();
	while (true)
	{
		auto desc = &rx_descriptors[rx_desc_read_index];
		if (desc->status & (1u << 31))
			break;

		const uint8_t* data = rx_buffers[rx_desc_read_index];
		size_t size = (desc->status & 0x3FFF0000) >> 16;
		rx_callback (data, size);

		desc->status |= (1u << 31);

		rx_desc_read_index++;
		if (rx_desc_read_index == rx_desc_count)
			rx_desc_read_index = 0;
	}

	__DSB();

	// In case we ran out of descriptors and the RX HW was suspended, wake it up.
	EMAC0->RXPOLLD = 0;
}

void* ethernet_transmit_get_buffer (size_t size)
{
	assert (size > 0);
	assert (size <= tx_buffer_size);

	__DSB();

	while (tx_descriptors[tx_desc_write_index].status & (1u << 31))
		;

	tx_descriptors[tx_desc_write_index].count = size;
	return tx_buffer;
}

void ethernet_transmit_release_buffer (void* buffer)
{
	assert (buffer == tx_buffer);

	bool last = (tx_desc_write_index == tx_desc_count - 1);

	tx_descriptors[tx_desc_write_index].status =
	      (1u << 31)  // OWN  - Owned by DMA
		| (1u << 30)  // IC   - Interrupt on Completion
		| (1u << 29)  // LS   - Last Segment of the frame
		| (1u << 28)  // FS   - First Segment of a frame
		| (3u << 22)  // CIC  - 3 = all checksums
		| (last ? (1u << 21) : 0) // TER - Transmit End of Ring
		| (1u << 20); // TCH  - Second Address Chained

	__DSB();

	// Tell the DMA to reacquire the descriptor now that we've filled it in.
	EMAC0->TXPOLLD = 0;

	tx_desc_write_index = (last ? 0 : (tx_desc_write_index + 1));
}

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
	while (EMAC0->DMABUSMOD & 1)
		;

    // Make sure that the DMA software reset is clear before continuing.
    while (EMAC0->DMABUSMOD & 1);

	EMAC0->DMABUSMOD = (1 << 26) // MB
		| (4 << 8) // Programmable Burst Length
		| (1 << 1); // DMA Arbitration Scheme (Fixed Prio)

	// The frequency of the System Clock is 100 to 150 MHz providing a MDIO clock of SYSCLK/62.
	EMAC0->MIIADDR = EMAC0->MIIADDR & (~(15 << 2) | (1 << 2));

    // Disable all the MMC interrupts as these are enabled by default at reset.
	EMAC0->MMCRXIM = 0xFFFFFFFF;
    EMAC0->MMCTXIM = 0xFFFFFFFF;

	EMAC0->CFG = (1 << 11) | (1 << 10); // DUPM and IPC (Checksum Offload Enable)
	EMAC0->DMAOPMODE = (1 << 25) | (1 << 21); // RSF and TSF

	// ----------------------------------------------------------
	// Initialize descriptors.

	for (size_t i = 0; i < tx_desc_count; i++)
	{
		tx_descriptors[i].status = 0;
		tx_descriptors[i].count = sizeof(tx_buffer);
		tx_descriptors[i].buffer1 = tx_buffer;
		tx_descriptors[i].next = &tx_descriptors[(i + 1) % tx_desc_count];
	}

	for (size_t i = 0; i < rx_desc_count; i++)
	{
		rx_descriptors[i].status = (1u << 31);
		rx_descriptors[i].count = (1 << 14) | rx_buffer_size;
		rx_descriptors[i].buffer1 = &rx_buffers[i];
		rx_descriptors[i].next = &rx_descriptors[(i + 1) % rx_desc_count];
	}

	__DSB();

    EMAC0->RXDLADDR = (uint32_t)rx_descriptors;
	EMAC0->TXDLADDR = (uint32_t)tx_descriptors;

	tx_desc_write_index = 0;
	rx_desc_read_index = 0;

	// ----------------------------------------------------------

	// Program the hardware with its MAC address (for filtering). Note that we must set
	// the registers in this order since the address is latched internally on the write to EMAC_O_ADDRL.
	EMAC0->ADDR0H = (mac_address[0] << 8) | mac_address[1];
	EMAC0->ADDR0L = (mac_address[2] << 24) | (mac_address[3] << 16) | (mac_address[4] << 8) | mac_address[5];

	// Set MAC filtering options.  We receive all broadcast and multicast
	// packets along with those addressed specifically for us.
	EMAC0->FRAMEFLTR = (1 << 9) | (1 << 4); // SAF (Source Address Filter) and PM (Pass All Multicast)

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
	// Enable the Ethernet MAC transmitter and receiver.
	EMAC0->DMAOPMODE |= (1 << 13) | (1 << 1); // set ST and SR
	EMAC0->CFG |= (1 << 3) | (1 << 2);        // set TE and RE

	//NVIC_EnableIRQ(EMAC0_IRQn);
	//EMAC0->DMAIM |= (1 << 6) | (1 << 16); // set RIE (Receive Interrupt Enable) and NIE
}

void ethernet_get_device_address (uint8_t addr[6])
{
	addr[0] = EMAC0->ADDR0H >> 8;
	addr[1] = EMAC0->ADDR0H;
    addr[2] = EMAC0->ADDR0L >> 24;
	addr[3] = EMAC0->ADDR0L >> 16;
	addr[4] = EMAC0->ADDR0L >> 8;
	addr[5] = EMAC0->ADDR0L;
}
