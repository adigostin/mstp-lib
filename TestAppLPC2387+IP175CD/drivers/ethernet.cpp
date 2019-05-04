
#include "ethernet.h"
#include "ethernet_defs.h"
#include "vic.h"
#include "event_queue.h"
#include <nxp/iolpc2387.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void ethernet_isr();
static void on_rx_event();

static ethernet_receive_handler rx_handler;
static bool rx_event_posted;

// Max allowed size on the wire is 1536, but we will tell the controller to add 4 CRC bytes
// and the switch IC to add 4 more, so the max allowed size in this point is 1536 - 4 - 4.
// There's a lot to be changed in the driver if we want frames 1536 bytes long in software.
#define MAX_FRAME_SIZE_IN_SOFTWARE	(1536 - 4 - 4)

#pragma segment="EMAC_DMA_RAM"

// ============================================================================
// TX

#define TX_BUFFER_COUNT		3

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=16
__no_init static unsigned char txBuffers [TX_BUFFER_COUNT] [MAX_FRAME_SIZE_IN_SOFTWARE];

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=8
__no_init static EnetDmaTxDesc_t EnetDmaTx [TX_BUFFER_COUNT];

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=8
__no_init static EnetDmaTxStatus_t EnetDmaTxSta [TX_BUFFER_COUNT];

// ============================================================================
// RX

#define RX_BUFFER_COUNT		7

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=16
__no_init static unsigned char rxBuffers [RX_BUFFER_COUNT] [MAX_FRAME_SIZE_IN_SOFTWARE];

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=8
__no_init static EnetDmaRxDesc_t EnetDmaRx [RX_BUFFER_COUNT];

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=8
__no_init static EnetDmaRxStatus_t EnetDmaRxSta [RX_BUFFER_COUNT];

void ethernet_init (const uint8_t macAddress[6], ethernet_receive_handler rx_handler)
{
	::rx_handler = rx_handler;

	unsigned int tout;

	for (tout = 100; tout; tout--);

	// Power Up the EMAC controller.
	PCONP_bit.PCENET = 1;

	// Hold on a while while the ethernet controller is initialized after it was powered.
	for (volatile int i = 0; i < 100000; i++)
		;

	assert (PINMODE2_bit.P1_0 == 2);
	assert (PINSEL2_bit.P1_0 == 1);

	// Reset all EMAC internal modules.
	MAC1 = (1 << 8) // MAC1_RES_TX
			| (1 << 9) // MAC1_RES_MCS_TX
			| (1 << 10) // MAC1_RES_RX
			| (1 << 11) // MAC1_RES_MCS_RX
			| (1 << 14) // MAC1_SIM_RES
			| (1 << 15); // MAC1_SOFT_RES

	COMMAND = (1 << 3) // CR_REG_RES
			| (1 << 4) // CR_TX_RES
			| (1 << 5); // CR_RX_RES

	// A short delay after reset.
	for (tout = 100; tout; tout--);

	// ------------------------------------------------------------------------

	// Initialize MAC control registers.
	MAC1 = (1 << 1); // MAC1_PASS_ALL; - this actually enables/reenables the MII interface
	MAXF = 0x600; // ETH_MAX_FLEN;

	CLRT = 0x0000370F;
	IPGR = 0x0C12;
	IPGT = 0x15;

	MAC2_bit.FD = COMMAND_bit.FULLDUPLEX = 1;

	MAC2_bit.PADCRCEN = 1; // enabled padding/CRC
	MAC2_bit.ADPE = 1; // auto-detect pad enable

	// This bit configures the Reduced MII logic for the current operating speed. When set,
	// 100 Mbps mode is selected. When cleared, 10 Mbps mode is selected.
	SUPP_bit.SPEED = 1;

//	// MIIM init
//	MCFG = 0x8018;  // clk/20
//	MCMD = 0;
//	MCFG_bit.RSTMIIMGMT = 0;
	COMMAND_bit.RMII = 1;

	// ----------------------------------------
	// init RX FIFO

	for (size_t i = 0; i < RX_BUFFER_COUNT; i++)
	{
		EnetDmaRx [i].EnetRxCtrl.data = (MAX_FRAME_SIZE_IN_SOFTWARE - 1) | (1u << 31);
		EnetDmaRx [i].pBuffer = rxBuffers [i];
		EnetDmaRxSta [i].Data [0] = 0;
		EnetDmaRxSta [i].Data [1] = 0;
	}

	// Set EMAC Receive Descriptor Registers.
	RXDESCRIPTOR   = (unsigned int) EnetDmaRx;
	RXSTATUS       = (unsigned int) EnetDmaRxSta;
	RXDESCRIPTORNUMBER = RX_BUFFER_COUNT - 1;
	// Rx Descriptors Point to 0
	RXCONSUMEINDEX  = 0;

	// ----------------------------------------
	// init TX FIFO

	for (unsigned int i = 0; i < TX_BUFFER_COUNT; i++)
		EnetDmaTx [i].pBuffer = txBuffers [i];

	// Set EMAC Transmit Descriptor Registers.
	TXDESCRIPTOR   = (unsigned int) EnetDmaTx;
	TXSTATUS       = (unsigned int) EnetDmaTxSta;
	TXDESCRIPTORNUMBER = TX_BUFFER_COUNT - 1;
	// Tx Descriptors Point to 0
	TXPRODUCEINDEX  = 0;

	// write the station address registers
	SA0 = (macAddress[1] << 8) | macAddress[0];
	SA1 = (macAddress[3] << 8) | macAddress[2];
	SA2 = (macAddress[5] << 8) | macAddress[4];

	// ----------------------------------------

	// Reset all interrupts
	INTCLEAR = 0xffffffff;

	// Enable EMAC interrupts.
	INTENABLE = (0 << 0) // rx overrun
			  | (0 << 1) // rx error
			  | (0 << 2) // rx finished (overflow)
			  | (1 << 3) // rx done
			  | (0 << 4) // tx underrun
			  | (0 << 5) // tx error
			  | (0 << 6) // tx finished
			  | (1 << 7);// tx done

	RXFILTERCTRL = (1 << 1)  // accept broadcast
				 | (1 << 2)  // accept multicast
				 | (1 << 5); // accept perfect match

	COMMAND_bit.RXENABLE = 1;
	MAC1_bit.RE = 1;
	COMMAND_bit.TXENABLE = 1;

	VIC_SetVectoredIRQ (ethernet_isr, 5, VIC_ETHERNET);

	// TODO: Send two frames with one fragment each - to account for the TXCONSUMEINDEX bug specified in the errata sheet.
}

// ============================================================================

static void ethernet_isr()
{
	uint32_t intstatus = INTSTATUS;
	intstatus &= INTENABLE;

	if (intstatus & (1 << 3))
	{
		// rx done
		INTCLEAR = (1 << 3);

		if (!rx_event_posted)
		{
			rx_event_posted = event_queue_try_push (on_rx_event, "on_rx_event");
			if (!rx_event_posted)
			{
				// TODO: handle this somehow
				printf ("!rx_event_posted\r\n");
			}
		}
	}

	if (intstatus & ((1 << 5) | (1 << 7)))
	{
		// TX error or TX done

		if (intstatus & (1 << 5))
			printf ("tx error\r\n");

		if ((intstatus & (1 << 7)) && (intstatus & (1 << 5)))
		{
			// tx error and tx done
			INTCLEAR = (1 << 5) | (1 << 7);
		}
		else if (intstatus & (1 << 5))
		{
			// tx error
			INTCLEAR = (1 << 5);
		}
		else if (intstatus & (1 << 7))
		{
			// tx done
			INTCLEAR = (1 << 7);
		}
	}
}

// ============================================================================

#pragma diag_suppress=Pa082 // order of volatile access
static void on_rx_event()
{
	rx_event_posted = false;

	while (RXCONSUMEINDEX != RXPRODUCEINDEX)
	{
		uint32_t index = RXCONSUMEINDEX;
		const EnetDmaRxStatus_t* status = &EnetDmaRxSta[index];
		if (status->FailFilter)
		{
			printf ("FailFilter\r\n");
		}
		else if (status->NoDescriptor)
		{
			printf ("NoDesc\r\n");
		}
		else if (status->AlignmentError)
		{
			printf ("Alignment\r\n");
		}
		else if (status->LengthError)
		{
			printf ("Length\r\n");
		}
		else if (status->SymbolError)
		{
			printf ("Symbol\r\n");
		}
		else if (status->CRCError)
		{
			printf ("CRC\r\n");
		}
		else if (status->Overrun)
		{
			printf ("Overrun\r\n");
		}
		else
		{
			uint32_t frameSize = status->RxSize + 1;
			assert (frameSize >= 64);

			// strip the CRC at the end
			frameSize -= 4;

			rx_handler (EnetDmaRx[index].pBuffer, frameSize);
		}

		index++;
		if (index == RX_BUFFER_COUNT)
			index = 0;
		RXCONSUMEINDEX = index;
	}
}
#pragma diag_default=Pa082

// ============================================================================

void ethernet_send (void* pPacket, unsigned int size)
{
	assert ((size > 0) && (size <= MAX_FRAME_SIZE_IN_SOFTWARE));

	unsigned int nextProduceIndex;

	do
	{
		nextProduceIndex = TXPRODUCEINDEX + 1;
		if (nextProduceIndex == TX_BUFFER_COUNT)
			nextProduceIndex = 0;
	} while (nextProduceIndex == TXCONSUMEINDEX);

	EnetDmaTxDesc_t* descriptor = &EnetDmaTx [TXPRODUCEINDEX];

	memcpy (descriptor->pBuffer, pPacket, size);
	descriptor->EnetTxCtrl.Data =
		  (size - 1)
		| (1U << 26) // Override
		| (0U << 27) // Huge
		| (1U << 28) // Pad
		| (1U << 29) // CRC
		| (1U << 30) // Last
		| (1U << 31);// Interrupt - needs to be set to avoid a hardware bug, see https://www.embeddedrelated.com/showthread/lpc2000/49876-1.php

	TXPRODUCEINDEX = nextProduceIndex;
}

// ============================================================================

void ENET_MIIWriteRegister (unsigned char DevId, unsigned char RegAddr, unsigned short Value)
{
	MCMD = 0;                   // set read operation
	MADR_bit.PHY_ADDR = DevId;  // set the MII Physical address
	MADR_bit.REGADDR = RegAddr; // set the MII register address
	MWTD = Value;
	while(MIND_bit.BUSY)
		;
}

// ============================================================================

#define MIND_BUSY        (1u<<0)
#define MIND_NOT_VALID   (1u<<2)

unsigned short ENET_MIIReadRegister (unsigned char DevId, unsigned char RegAddr)
{
	MCMD = 0;
	MADR_bit.PHY_ADDR = DevId;  // set the MII Physical address
	MADR_bit.REGADDR = RegAddr; // set the MII register address
	MCMD = 1;                   // set read operation
	while(MIND &(MIND_BUSY | MIND_NOT_VALID))
		;

	return (unsigned short) MRDD;
}


