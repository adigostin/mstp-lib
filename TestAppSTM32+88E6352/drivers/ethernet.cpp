
#include "ethernet.h"
#include "assert.h"
#include "clock.h"
#include "scheduler.h"
#include "mpu.h"
#include "event_queue.h"
#include "serial_console.h"
#include <string.h>
#include <stdio.h>

static bool enet_initialized;
static ethernet_frame_received_t frame_received;
static uint32_t phy_id;
static bool phy_error_printed;
static bool dump_received_packets;

struct rx_descriptor
{
	volatile uint32_t  Status;
	uint32_t           ControlBufferSize;
	uint8_t*           Buffer1Addr;
	rx_descriptor*     Buffer2NextDescAddr;
	uint32_t unused1;
	uint32_t unused2;
	uint32_t unused3;
	uint32_t unused4;
};

struct tx_descriptor
{
	volatile uint32_t  Status;
	uint32_t           ControlBufferSize;
	uint8_t*           Buffer1Addr;
	tx_descriptor*     Buffer2NextDescAddr;
	uint32_t unused1;
	uint32_t unused2;
	uint32_t unused3;
	uint32_t unused4;
};

static rx_descriptor* rx_descriptor_read_ptr;
static tx_descriptor* tx_descriptor_write_ptr;

static ethernet_diags diags;

namespace {
	extern const serial_command serial_commands[];
}

static void ETH_MACTransmissionEnable();
static void ETH_MACTransmissionDisable();
static void ETH_MACReceptionEnable();
static void ETH_MACReceptionDisable();
static void ETH_FlushTransmitFIFO();

static void HAL_ETH_Start();
static void HAL_ETH_Stop();
static void HAL_ETH_DMATxDescListInit (tx_descriptor *DMATxDescTab, uint8_t *TxBuff, uint32_t TxBuffCount);
static void HAL_ETH_DMARxDescListInit (rx_descriptor *DMARxDescTab, uint8_t *RxBuff, uint32_t RxBuffCount);

#define ETH_MAX_PACKET_SIZE    ((uint32_t)1524U)    /*!< ETH_HEADER + ETH_EXTRA + ETH_VLAN_TAG + ETH_MAX_ETH_PAYLOAD + ETH_CRC */
#define ETH_HEADER               ((uint32_t)14U)    /*!< 6 byte Dest addr, 6 byte Src addr, 2 byte length/type */
#define ETH_CRC                   ((uint32_t)4U)    /*!< Ethernet CRC */
#define ETH_EXTRA                 ((uint32_t)2U)    /*!< Extra bytes in some cases */
#define ETH_VLAN_TAG              ((uint32_t)4U)    /*!< optional 802.1q VLAN Tag */
#define ETH_MIN_ETH_PAYLOAD       ((uint32_t)46U)    /*!< Minimum Ethernet payload size */
#define ETH_MAX_ETH_PAYLOAD       ((uint32_t)1500U)    /*!< Maximum Ethernet payload size */
#define ETH_JUMBO_FRAME_PAYLOAD   ((uint32_t)9000U)    /*!< Jumbo frame payload size */

#define ETH_RX_BUF_SIZE         ETH_MAX_PACKET_SIZE
#define ETH_RXBUFNB             4
#define ETH_TX_BUF_SIZE         ETH_MAX_PACKET_SIZE
#define ETH_TXBUFNB             4

#if defined ( __CC_ARM   )
rx_descriptor  DMARxDscrTab[ETH_RXBUFNB] __attribute__((at(0x2007C000)));/* Ethernet Rx DMA Descriptors */

tx_descriptor  DMATxDscrTab[ETH_TXBUFNB] __attribute__((at(0x2007C080)));/* Ethernet Tx DMA Descriptors */

uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__((at(0x2007C100))); /* Ethernet Receive Buffers */

uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__((at(0x2007D8D0))); /* Ethernet Transmit Buffers */

#elif defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4

#pragma location=0x2007C000
__no_init rx_descriptor  DMARxDscrTab[ETH_RXBUFNB];/* Ethernet Rx DMA Descriptors */

#pragma location=0x2007C080
__no_init tx_descriptor  DMATxDscrTab[ETH_TXBUFNB];/* Ethernet Tx DMA Descriptors */

#pragma location=0x2007C100
__no_init uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE]; /* Ethernet Receive Buffers */

#pragma location=0x2007D8D0
__no_init uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE]; /* Ethernet Transmit Buffers */

#elif defined ( __GNUC__ ) /*!< GNU Compiler */

rx_descriptor  DMARxDscrTab[ETH_RXBUFNB] __attribute__((section(".sram2")));/* Ethernet Rx DMA Descriptors */

tx_descriptor  DMATxDscrTab[ETH_TXBUFNB] __attribute__((section(".sram2")));/* Ethernet Tx DMA Descriptors */

uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__((section(".sram2"))); /* Ethernet Receive Buffers */

uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__((section(".sram2"))); /* Ethernet Transmit Buffers */

#endif

extern char __SRAM2_segment_start__;
extern char __SRAM2_segment_end__;


static void MPU_Config()
{
	MPU_Region_InitTypeDef MPU_InitStruct;

	// TODO: clean this up

	// Disable the MPU
	mpu_disable();

	// Configure the MPU as Normal Non Cacheable for Ethernet Buffers in the SRAM2.
	assert (&__SRAM2_segment_end__ - &__SRAM2_segment_start__ == 0x4000);
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress = 0x2007C000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_16KB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.Number = MPU_REGION_NUMBER1;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
	mpu_config_region (&MPU_InitStruct);

	// Configure the MPU as Device for Ethernet Descriptors in the SRAM2.
	assert (((uint32_t)DMARxDscrTab >= 0x2007C000) && ((uint32_t)DMARxDscrTab < 0x2007C000 + 256));
	assert (((uint32_t)DMATxDscrTab >= 0x2007C000) && ((uint32_t)DMATxDscrTab < 0x2007C000 + 256));
	assert ((uint32_t)Rx_Buff >= 0x2007C000 + 256);
	assert ((uint32_t)Tx_Buff >= 0x2007C000 + 256);
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress = 0x2007C000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.Number = MPU_REGION_NUMBER2;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
	mpu_config_region (&MPU_InitStruct);

	// Enable the MPU
	mpu_enable (MPU_PRIVILEGED_DEFAULT);
}

bool enet_init (const struct ethernet_pins& pins, const uint8_t mac_address[6], ethernet_frame_received_t frame_received)
{
	assert (!enet_initialized);
	assert ((__get_PRIMASK() & 1) == 0); // this function is lengthy so is must not be called in interrupt mode

	assert (clock_get_ahb_freq() >= 25'000'000); // according to note at 42.4 Ethernet functional description: SMI, MII and RMII

	::frame_received = frame_received;
	::phy_id = 0xFFFF'FFFF;
	::phy_error_printed = false;

	gpio_make_alternate (pins.rmii_ref_clk, pin_output_speed_t::very_high);
	gpio_make_alternate (pins.rmii_mdio,    pin_output_speed_t::medium);
	gpio_make_alternate (pins.rmii_mdc,     pin_output_speed_t::medium);
	gpio_make_alternate (pins.rmii_crs_dv,  pin_output_speed_t::very_high);
	gpio_make_alternate (pins.rmii_rxd0,    pin_output_speed_t::very_high);
	gpio_make_alternate (pins.rmii_rxd1,    pin_output_speed_t::very_high);
	gpio_make_alternate (pins.rmii_tx_en,   pin_output_speed_t::very_high);
	gpio_make_alternate (pins.rmii_txd0,    pin_output_speed_t::very_high);
	gpio_make_alternate (pins.rmii_txd1,    pin_output_speed_t::very_high);

	clock_enable(ETH);

	// TODO: set mac address
	/*
	EthHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
	EthHandle.Init.Speed = ETH_SPEED_100M;
	EthHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
	EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
	EthHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
	EthHandle.Init.ChecksumMode = 0;//ETH_CHECKSUM_BY_HARDWARE;
	EthHandle.Init.PhyAddress = LAN8742A_PHY_ADDRESS;
	*/
	clock_enable(SYSCFG);

	SYSCFG->PMC |= SYSCFG_PMC_MII_RMII_SEL;

	// Ethernet Software reset.
	// Set the SWR bit: resets all MAC subsystem internal registers and logic.
	// After reset all the registers holds their respective reset values.
	ETH->DMABMR |= ETH_DMABMR_SR;

	// Wait for the hardware to perform the reset.
	uint32_t time_start = scheduler_get_time_ms32();
	while ((ETH->DMABMR & ETH_DMABMR_SR) != 0)
	{
		if (scheduler_get_time_ms32() - time_start >= 500)
		{
			// TODO: undo what we've done so far.
			return false;
		}
	}

	NVIC_EnableIRQ(ETH_IRQn);

	//-------------------------------- MAC Initialization ----------------------
	uint32_t tempreg = ETH->MACMIIAR;
	// Clear CSR Clock Range CR[2:0] bits
	tempreg &= ~ETH_MACMIIAR_CR_Msk;

	/* Get hclk frequency value */
	auto hclk = clock_get_sysclk_freq();

	// TODO: sanitize
	/* Set CR bits depending on hclk value */
	if ((hclk >= 20'000'000) && (hclk < 35'000'000)) {
		/* CSR Clock Range between 20-35 MHz */
		tempreg |= (uint32_t)ETH_MACMIIAR_CR_Div16;
	} else if ((hclk >= 35'000'000) && (hclk < 60'000'000)) {
		/* CSR Clock Range between 35-60 MHz */
		tempreg |= (uint32_t)ETH_MACMIIAR_CR_Div26;
	} else if ((hclk >= 60'000'000) && (hclk < 100'000'000)) {
		/* CSR Clock Range between 60-100 MHz */
		tempreg |= (uint32_t)ETH_MACMIIAR_CR_Div42;
	} else if ((hclk >= 100'000'000) && (hclk < 150'000'000)) {
		/* CSR Clock Range between 100-150 MHz */
		tempreg |= (uint32_t)ETH_MACMIIAR_CR_Div62;
	} else /* ((hclk >= 150'000'000)&&(hclk <= 216'000'000)) */
	{
		/* CSR Clock Range between 150-216 MHz */
		tempreg |= (uint32_t)ETH_MACMIIAR_CR_Div102;
	}

	/* Write to ETHERNET MAC MIIAR: Configure the ETHERNET CSR Clock Range */
	ETH->MACMIIAR = (uint32_t)tempreg;
	scheduler_wait(2);

	ETH->MACCR  |= ETH_MACCR_FES | ETH_MACCR_DM;// | ETH_MACCR_TE | ETH_MACCR_RE;
	// sample app writes 0x0000'CE00
	// Wait 4 TX/RX cycles after setting this register. (Actually necessary?)
	scheduler_wait(2);

	ETH->MACFFR = 0; // receive only frames that pass the perfect DA filter
	ETH->MACA0HR = (mac_address[5] << 8) | mac_address[4];
	ETH->MACA0LR = (mac_address[3] << 24) | (mac_address[2] << 16) | (mac_address[1] << 8) | mac_address[0];
	ETH->MACA1HR = (1u << 31);
	ETH->MACA1LR = (0xC2 << 16) | (0x80 << 8) | 0x01;

	// sample app writes 0x0000'0040
	// Wait 4 TX/RX cycles after setting this register. (Actually necessary?)
	scheduler_wait(2);

	// TODO: maybe set MACFCR, wait after, sample app writes 0x80

	// See 42.6.1 Initialization of a transfer using DMA

	// TODO: maybe set DMABMR, wait after, sample app writes 0x02C1'2080

	ETH->DMAIER |= ETH_DMAIER_NISE | ETH_DMAIER_RIE;

	ETH->DMAOMR = (1 << 25) // RSF
				| (1 << 21) // TSF
				| (1 << 2); // OSF

	MPU_Config();

	// Initialize Tx Descriptors list: Chain Mode
	HAL_ETH_DMATxDescListInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);

	// Initialize Rx Descriptors list: Chain Mode
	HAL_ETH_DMARxDescListInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

	// Enable MAC and DMA transmission and reception.
	HAL_ETH_Start();

	// Check if it's a STM32F76xxx Revision A
	uint32_t revid = (DBGMCU->IDCODE) >> 16;
	if(revid == 0x1000)
	{
		// Partial workaround for the Ethernet erroneous data received in RMII configuration Hardware Bug
		// (please refer to the STM32F76xxx STM32F77xxx Errata sheet)
		// This thread will keep resetting the RMII interface until good frames are received
		assert(false);
		//ctl_task_run (&rmii_task, 12, RMII_Thread, nullptr, "rmii_task", sizeof(rmii_task_stack) / 4, rmii_task_stack, 0);
	}

	serial_console_register_command_set (serial_commands);
	enet_initialized = true;
	return true;
}

bool enet_is_init()
{
	return enet_initialized;
}

void enet_get_mac_address (uint8_t mac_address[6])
{
	mac_address[0] = ETH->MACA0LR & 0xff;
	mac_address[1] = (ETH->MACA0LR >> 8) & 0xff;
	mac_address[2] = (ETH->MACA0LR >> 16) & 0xff;
	mac_address[3] = (ETH->MACA0LR >> 24) & 0xff;
	mac_address[4] = ETH->MACA0HR & 0xff;
	mac_address[5] = (ETH->MACA0HR >> 8) & 0xff;
}

static void HAL_ETH_Start()
{
	ETH_MACTransmissionEnable();
	ETH_MACReceptionEnable();
	ETH_FlushTransmitFIFO();
	ETH->DMAOMR |= (ETH_DMAOMR_ST | ETH_DMAOMR_SR); // Enable DMA transmission and reception
}

static void HAL_ETH_Stop()
{
	ETH->DMAOMR &= ~(ETH_DMAOMR_ST | ETH_DMAOMR_SR); // Disable DMA transmission and reception
	ETH_MACReceptionDisable();
	ETH_FlushTransmitFIFO();
	ETH_MACTransmissionDisable();
}


static void HAL_ETH_DMATxDescListInit (tx_descriptor* DMATxDescTab, uint8_t *TxBuff, uint32_t TxBuffCount)
{
	for (size_t i = 0; i < TxBuffCount; i++)
	{
		/* Get the pointer on the ith member of the Tx Desc list */
		auto dmatxdesc = DMATxDescTab + i;

		/* Set Second Address Chained bit */
		dmatxdesc->Status = 0x00100000;

		// TODO: set interrupt flag similar to that from the rx descriptor

		/* Set Buffer1 address pointer */
		dmatxdesc->Buffer1Addr = &TxBuff[i*ETH_TX_BUF_SIZE];

		// Set the DMA Tx descriptors checksum insertion.
		dmatxdesc->Status |= (3 << 22); // CIC field = 11;

		/* Initialize the next descriptor with the Next Descriptor Polling Enable */
		if(i < (TxBuffCount-1))
		{
			/* Set next descriptor address register with next descriptor base address */
			dmatxdesc->Buffer2NextDescAddr = DMATxDescTab+i+1;
		}
		else
		{
			/* For last descriptor, set next descriptor address register equal to the first descriptor base address */
			dmatxdesc->Buffer2NextDescAddr = DMATxDescTab;
		}
	}

	tx_descriptor_write_ptr = DMATxDescTab;

	// Set Transmit Descriptor List Address Register.
	ETH->DMATDLAR = (uint32_t) DMATxDescTab;
}

static void HAL_ETH_DMARxDescListInit (rx_descriptor* DMARxDescTab, uint8_t *RxBuff, uint32_t RxBuffCount)
{
	for (size_t i = 0; i < RxBuffCount; i++)
	{
		/* Get the pointer on the ith member of the Rx Desc list */
		auto DMARxDesc = DMARxDescTab + i;

		/* Set Own bit of the Rx descriptor Status */
		DMARxDesc->Status = 0x8000'0000; // ETH_DMARXDESC_OWN;

		/* Set Buffer1 size and Second Address Chained bit */
		DMARxDesc->ControlBufferSize = 0x00004000U | ETH_RX_BUF_SIZE; //ETH_DMARXDESC_RCH | ETH_RX_BUF_SIZE;

		/* Set Buffer1 address pointer */
		DMARxDesc->Buffer1Addr = &RxBuff[i*ETH_RX_BUF_SIZE];

		/* Enable Ethernet DMA Rx Descriptor interrupt */
		DMARxDesc->ControlBufferSize &= ~0x80000000U;//ETH_DMARXDESC_DIC;

		/* Initialize the next descriptor with the Next Descriptor Polling Enable */
		if(i < (RxBuffCount-1))
		{
			/* Set next descriptor address register with next descriptor base address */
			DMARxDesc->Buffer2NextDescAddr = DMARxDescTab+i+1;
		}
		else
		{
			/* For last descriptor, set next descriptor address register equal to the first descriptor base address */
			DMARxDesc->Buffer2NextDescAddr = DMARxDescTab;
		}
	}

	rx_descriptor_read_ptr = DMARxDescTab;

	// Set Receive Descriptor List Address Register
	ETH->DMARDLAR = (uint32_t) DMARxDescTab;
}


static void ETH_MACTransmissionEnable()
{
  __IO uint32_t tmpreg = 0;

  /* Enable the MAC transmission */
  ETH->MACCR |= ETH_MACCR_TE;

  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = ETH->MACCR;
	scheduler_wait(2);
  ETH->MACCR = tmpreg;
}

static void ETH_MACTransmissionDisable()
{
  __IO uint32_t tmpreg = 0;

  /* Disable the MAC transmission */
  ETH->MACCR &= ~ETH_MACCR_TE;

  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = ETH->MACCR;
	scheduler_wait(2);
  ETH->MACCR = tmpreg;
}

static void ETH_MACReceptionEnable()
{
  __IO uint32_t tmpreg = 0;

  /* Enable the MAC reception */
  ETH->MACCR |= ETH_MACCR_RE;

  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = ETH->MACCR;
	scheduler_wait(2);
  ETH->MACCR = tmpreg;
}

static void ETH_MACReceptionDisable()
{
  __IO uint32_t tmpreg = 0;

  /* Disable the MAC reception */
  ETH->MACCR &= ~ETH_MACCR_RE;

  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = ETH->MACCR;
	scheduler_wait(2);
  ETH->MACCR = tmpreg;
}

static void ETH_FlushTransmitFIFO()
{
  __IO uint32_t tmpreg = 0;

  /* Set the Flush Transmit FIFO bit */
  ETH->DMAOMR |= ETH_DMAOMR_FTF;

  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = ETH->DMAOMR;
	scheduler_wait(2);
  ETH->DMAOMR = tmpreg;
}

#define ETH_DMARXDESC_OWN         ((uint32_t)0x80000000U)  /*!< OWN bit: descriptor is owned by DMA engine  */
#define ETH_DMARXDESC_FS          ((uint32_t)0x00000200U)  /*!< First descriptor of the frame  */
#define ETH_DMARXDESC_LS          ((uint32_t)0x00000100U)  /*!< Last descriptor of the frame  */
#define ETH_DMARXDESC_FL          ((uint32_t)0x3FFF0000U)  /*!< Receive descriptor frame length  */
#define ETH_DMARXDESC_FRAMELENGTHSHIFT            ((uint32_t)16)

static uint8_t* get_received_frame_acquire (size_t* frame_size_out, size_t* descriptor_count_out)
{
	if (rx_descriptor_read_ptr->Status & ETH_DMARXDESC_OWN)
		return nullptr;

	assert (rx_descriptor_read_ptr->Status & ETH_DMARXDESC_FS);

	// a.t.m. we have large-enough buffers, so we should have a single segment.
	assert (rx_descriptor_read_ptr->Status & ETH_DMARXDESC_LS);

	*frame_size_out = ((rx_descriptor_read_ptr->Status & ETH_DMARXDESC_FL) >> ETH_DMARXDESC_FRAMELENGTHSHIFT) - 4;
	*descriptor_count_out = 1;
	return rx_descriptor_read_ptr->Buffer1Addr;
/*
    while ((::RxDesc->Status & ETH_DMARXDESC_OWN) == 0)
    {
        // if ((::RxDesc->Status & (ETH_DMARXDESC_FS | ETH_DMARXDESC_LS)) == (uint32_t)ETH_DMARXDESC_FS)
        {
            ::RxFrameInfos.FSRxDesc = ::RxDesc;
            ::RxFrameInfos.SegCount = 1;
            // Point to next descriptor
            ::RxDesc = (rx_descriptor*)(::RxDesc->Buffer2NextDescAddr);
        }
        // Check if intermediate segment
        else if ((::RxDesc->Status & (ETH_DMARXDESC_LS | ETH_DMARXDESC_FS)) == 0)
        {
            // Increment segment count
            (::RxFrameInfos.SegCount)++;
            // Point to next descriptor
            ::RxDesc = (rx_descriptor*)(::RxDesc->Buffer2NextDescAddr);
        }
        // Should be last segment
        else
        {
            // Last segment
            ::RxFrameInfos.LSRxDesc = ::RxDesc;

            // Increment segment count
            (::RxFrameInfos.SegCount)++;

            // Check if last segment is first segment: one segment contains the frame
            if ((::RxFrameInfos.SegCount) == 1)
            {
                ::RxFrameInfos.FSRxDesc = ::RxDesc;
            }

            // Get the Frame Length of the received packet: substruct 4 bytes of the CRC
            ::RxFrameInfos.length = (((::RxDesc)->Status & ETH_DMARXDESC_FL) >> ETH_DMARXDESC_FRAMELENGTHSHIFT) - 4;

            // Get the address of the buffer start address
            ::RxFrameInfos.buffer = ((::RxFrameInfos).FSRxDesc)->Buffer1Addr;

            // Point to next descriptor
            ::RxDesc = (rx_descriptor*)(::RxDesc->Buffer2NextDescAddr);
        }
    }
	*/
}

static void get_received_frame_release (size_t descriptor_count)
{
	while (descriptor_count--)
	{
		rx_descriptor_read_ptr->Status |= ETH_DMARXDESC_OWN;
		rx_descriptor_read_ptr = rx_descriptor_read_ptr->Buffer2NextDescAddr;
	}
}

static void process_received_frames()
{
	while (true)
	{
		size_t frame_size;
		size_t descriptor_count;
		auto buffer = get_received_frame_acquire (&frame_size, &descriptor_count);
		if (buffer == nullptr)
			break;

		if (dump_received_packets)
		{
			printf ("rx: ");
			enet_dump_frame (buffer, frame_size);
		}

		::frame_received(buffer, frame_size);

		get_received_frame_release(descriptor_count);
	}
}

extern "C" void ETH_IRQHandler()
{
	if (ETH->DMASR & ETH_DMASR_RS)
	{
		// Frame received
		ETH->DMASR = ETH_DMASR_RS;

		bool pushed = event_queue_try_push (process_received_frames, "process_received_frames");
		if (!pushed)
		{
			// We can't push any more events at the moment, which means the mainline code
			// can't process frames as fast as they arrive. To keep the code simple,
			// let's discard all frames received and not yet processed.
			// (Maybe later I'll make this optimization: register a callback to be called when
			// the event queue becomes empty, and in that callback try again to push.)
			size_t descriptor_count;
// TODO: prevent process_received_frames() from running while in this loop
			while (get_received_frame_acquire (nullptr, &descriptor_count))
			{
				get_received_frame_release(descriptor_count);
				diags.frames_discarded_event_queue_full++;
			}
		}
	}

	if (ETH->DMASR & ETH_DMASR_TS)
	{
		// Frame transmitted
		ETH->DMASR = ETH_DMASR_TS;
	}

	// Clear the interrupt flags
	ETH->DMASR = ETH_DMASR_NIS;

	if (ETH->DMASR & ETH_DMASR_AIS)
	{
		// ETH DMA Error

		assert(false);

		// Clear the interrupt flags
		ETH->DMASR = ETH_DMASR_AIS;
	}
}

uint16_t enet_read_smi (uint16_t phy_address, uint16_t reg_number)
{
	assert (enet_initialized);

	uint32_t tmpreg = ETH->MACMIIAR;

	// Keep only the CSR Clock Range CR[2:0] bits value
	tmpreg &= ETH_MACMIIAR_CR_Msk;

	// Prepare the MII address register value
	tmpreg |=(((uint32_t)phy_address << 11) & ETH_MACMIIAR_PA);
	tmpreg |=(((uint32_t)reg_number <<  6) & ETH_MACMIIAR_MR);
	tmpreg &= ~ETH_MACMIIAR_MW; // read operation
	tmpreg |= ETH_MACMIIAR_MB; // busy bit

	// Write the address.
	ETH->MACMIIAR = tmpreg;

	// Check for the Busy flag
	uint32_t tickstart = scheduler_get_time_ms32();
	while((ETH->MACMIIAR & ETH_MACMIIAR_MB) == ETH_MACMIIAR_MB)
	{
		assert ((scheduler_get_time_ms32() - tickstart) < 100);
	}

	return (uint16_t)ETH->MACMIIDR;
}

void enet_write_smi (uint16_t phy_address, uint16_t reg_number, uint16_t value)
{
	assert (enet_initialized);

	auto tmpreg = ETH->MACMIIAR;

	// Keep only the CSR Clock Range CR[2:0] bits value.
	tmpreg &= ETH_MACMIIAR_CR_Msk;

	// Prepare the MII register address value
	tmpreg |=(((uint32_t)phy_address << 11) & ETH_MACMIIAR_PA);
	tmpreg |=(((uint32_t)reg_number  <<  6) & ETH_MACMIIAR_MR);
	tmpreg |= ETH_MACMIIAR_MW; // write operation
	tmpreg |= ETH_MACMIIAR_MB; // busy bit

	ETH->MACMIIDR = value;
	ETH->MACMIIAR = tmpreg;

	// Check for the Busy flag.
	uint32_t tickstart = scheduler_get_time_ms32();
	while((ETH->MACMIIAR & ETH_MACMIIAR_MB) == ETH_MACMIIAR_MB)
	{
		assert ((scheduler_get_time_ms32() - tickstart) < 100);
	}
}

#define ETH_DMATXDESC_OWN                     ((uint32_t)0x80000000U)  /*!< OWN bit: descriptor is owned by DMA engine */
#define ETH_DMATXDESC_LS                      ((uint32_t)0x20000000U)  /*!< Last Segment */
#define ETH_DMATXDESC_FS                      ((uint32_t)0x10000000U)  /*!< First Segment */
#define ETH_DMATXDESC_TBS2  ((uint32_t)0x1FFF0000U)  /*!< Transmit Buffer2 Size */
#define ETH_DMATXDESC_TBS1  ((uint32_t)0x00001FFFU)  /*!< Transmit Buffer1 Size */

void enet_send_blocking (const uint8_t* frame, size_t frame_len)
{
	assert ((__get_PRIMASK() & 1) == 0); // this function may not be called in interrupt mode

	assert (frame_len >= 14);

	// Wait for an empty descriptor (not owned by the DMA).
	while (tx_descriptor_write_ptr->Status & ETH_DMATXDESC_OWN)
		;

	assert (frame_len <= ETH_TX_BUF_SIZE);

	memcpy (tx_descriptor_write_ptr->Buffer1Addr, frame, frame_len);

	// Set LAST and FIRST segment
	tx_descriptor_write_ptr->Status |= ETH_DMATXDESC_FS | ETH_DMATXDESC_LS;
	// Set frame size
	tx_descriptor_write_ptr->ControlBufferSize = (frame_len & ETH_DMATXDESC_TBS1);
	// Set Own bit of the Tx descriptor Status: gives the buffer back to ETHERNET DMA
	tx_descriptor_write_ptr->Status |= ETH_DMATXDESC_OWN;
	// Point to next descriptor
	tx_descriptor_write_ptr = (tx_descriptor*)(tx_descriptor_write_ptr->Buffer2NextDescAddr);

	/* When Tx Buffer unavailable flag is set: clear it and resume transmission */
	if (ETH->DMASR & ETH_DMASR_TBUS)
	{
		// Clear TBUS ETHERNET DMA flag
		ETH->DMASR = ETH_DMASR_TBUS;
		// Resume DMA transmission
		ETH->DMATPDR = 0;
	}
}

void enet_dump_frame (const uint8_t* frame, size_t frame_len)
{
	printf ("%02x %02x %02x %02x %02x %02x   ", frame[0], frame[1], frame[2], frame[3], frame[4], frame[5]);
	printf ("%02x %02x %02x %02x %02x %02x   ", frame[6], frame[7], frame[8], frame[9], frame[10], frame[11]);
	printf ("%02x %02x   ", frame[12], frame[13]);
	size_t dump_len = (frame_len < 40) ? frame_len : 40;
	for (size_t i = 14; i < dump_len; i++)
		printf ("%02x ", frame[i]);
	printf ("\r\n");
}

static void process_read_mii_command (const char* params)
{
	uint32_t addr, reg;
	if (sscanf (params, "%u, %u", &addr, &reg) == 2)
	{
		uint16_t value = enet_read_smi ((uint16_t)addr, (uint16_t)reg);
		printf ("[Phy %u] [Reg %u] = 0x%04x (", addr, reg, value);
		print_binary(value);
		printf (")\r\n");
	}
	else
		printf ("bad params!\r\n");
}

static void process_write_mii_command (const char* params)
{
	uint32_t addr, reg, value;

	if (sscanf (params, "%d, %d, %x", &addr, &reg, &value) == 3)
	{
		enet_write_smi ((uint16_t)addr, (uint16_t)reg, (uint16_t)value);

		value = enet_read_smi ((uint16_t)addr, (uint16_t)reg);
		printf ("Written. Value read back = 0x%04x\r\n", value);
	}
	else
		printf ("bad params!\r\n");
}

static void print_rxcrc (const char*)
{
	printf ("ETH_MMCRFCECR = %d\r\n", ETH->MMCRFCECR);
}

static void process_dump_cmd (const char*)
{
	dump_received_packets = !dump_received_packets;
}

static void process_diags_cmd (const char*)
{
	printf ("ethernet diagnostics:\r\n");
	indent();
	printf ("frames_discarded_event_queue_full=%d\r\n", diags.frames_discarded_event_queue_full);
	unindent();
}

namespace
{
	const serial_command serial_commands[] =
	{
		{ "rm",		 "\"rm phy, reg\" - reads an MII register", process_read_mii_command },
		{ "wm",		 "\"wm phy, reg, hex_value\" - writes to an MII register", process_write_mii_command },
		{ "rxcrc",   "prints the number of frames received with CRC error", print_rxcrc },
		{ "dump",	 "toggles dumping of received Ethernet packets on/off", process_dump_cmd },
		{ "diags",   "", process_diags_cmd },
		{ nullptr,   nullptr, nullptr },
	};
}
