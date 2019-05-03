
#include <nxp/iolpc2387.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "LPC23xx_enet.h"

// ============================================================================
/*!
 * \brief EnetTxCtrl_t
 *
 * Transmit descriptor control word
 *
 * See LPC23xx User Manual Ch16, Table 212
 *
 */
typedef union _EnetTxCtrl_t
{
	unsigned int Data;
	struct
	{
		unsigned int Size     : 11;
		unsigned int 		  : 15;
		unsigned int Override :  1;
		unsigned int Huge     :  1;
		unsigned int Pad      :  1;
		unsigned int CRC      :  1;
		unsigned int Last     :  1;
		unsigned int Intr     :  1;
	};
} EnetTxCtrl_t, *pEnetTxCtrl_t;

// ============================================================================
/*!
 * \brief EnetTxCtrl_t
 *
 * Transmit descriptor
 *
 * See LPC23xx User Manual Ch16, Table 211
 *
 */
typedef struct _EnetDmaTxDesc_t
{
  unsigned char* pBuffer;
  EnetTxCtrl_t EnetTxCtrl;
} EnetDmaTxDesc_t, * pEnetDmaTxDesc_t;

// ============================================================================
/*!
 * \brief EnetDmaTxStatus_t
 *
 * Transmit status
 *
 * See LPC23xx User Manual Ch16, Table 211
 *
 */
typedef union _EnetDmaTxStatus_t
{
	unsigned int Data;
	struct
	{
		unsigned int                    :21;
		unsigned int CollisionCount     : 4;
		unsigned int Defer              : 1;
		unsigned int ExcessiveDefer     : 1;
		unsigned int ExcessiveCollision : 1;
		unsigned int LateCollision      : 1;
		unsigned int Underrun           : 1;
		unsigned int NoDescriptor       : 1;
		unsigned int Error              : 1;
	};
} EnetDmaTxStatus_t, * pEnetDmaTxStatus_t;

// ============================================================================
/*!
 * \brief EnetRxCtrl_t
 *
 * Receive descriptor control word
 *
 * See LPC23xx User Manual Ch16, Table 207
 *
 */
typedef struct _EnetRxCtrl_t
{
  unsigned int Size     : 11;
  unsigned int          : 20;
  unsigned int Intr     :  1;
} EnetRxCtrl_t, *pEnetRxCtrl_t;

// ============================================================================
/*!
 * \brief EnetDmaRxDesc_t
 *
 * Receive descriptor
 *
 * See LPC23xx User Manual Ch16, Table 206
 *
 */
typedef struct _EnetDmaRxDesc_t
{
  unsigned char* pBuffer;
  EnetRxCtrl_t EnetRxCtrl;
} EnetDmaRxDesc_t, * pEnetDmaRxDesc_t;

// ============================================================================
/*!
 * \brief EnetDmaRxStatus_t
 *
 * Receive status
 *
 * See LPC23xx User Manual Ch16, Table 208
 *
 */
typedef union _EnetDmaRxStatus_t
{
	unsigned int Data[2];
	struct
	{
		unsigned int RxSize         :11;
		unsigned int                : 7;
		unsigned int ControlFrame   : 1; // bit 18
		unsigned int VLAN           : 1; // bit 19

		unsigned int FailFilter     : 1; // bit 20
		unsigned int Multicast      : 1; // bit 21
		unsigned int Broadcast      : 1; // bit 22
		unsigned int CRCError       : 1; // bit 23

		unsigned int SymbolError    : 1; // bit 24
		unsigned int LengthError    : 1; // bit 25
		unsigned int RangeError     : 1; // bit 26
		unsigned int AlignmentError : 1; // bit 27

		unsigned int Overrun        : 1; // bit 28
		unsigned int NoDescriptor   : 1; // bit 29
		unsigned int LastFlag       : 1; // bit 30
		unsigned int Error          : 1; // bit 31

		unsigned int SAHashCRC      : 8;
		unsigned int                : 8;
		unsigned int DAHashCRC      : 8;
		unsigned int                : 8;
	};
} EnetDmaRxStatus_t, * pEnetDmaRxStatus_t;

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

void ENET_Init (const unsigned char macAddress[6])
{
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

	for (unsigned int i = 0; i < RX_BUFFER_COUNT; i++)
	{
		EnetDmaRx [i].EnetRxCtrl.Size = MAX_FRAME_SIZE_IN_SOFTWARE - 1;
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

	RXFILTERCTRL = (1 << 1)  // accept broadcast
				 | (1 << 2)  // accept multicast
				 | (1 << 5); // accept perfect match

	COMMAND_bit.RXENABLE = 1;
	MAC1_bit.RE = 1;
	COMMAND_bit.TXENABLE = 1;
}

// ============================================================================

unsigned int tapdev_read (void* pPacket)
{
	unsigned int index = RXCONSUMEINDEX;
	if (index == RXPRODUCEINDEX)
		return 0;

	unsigned int frameSize = 0;

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
		frameSize = status->RxSize + 1;
		assert (frameSize >= 64);

		// strip the CRC at the end
		frameSize -= 4;

		memcpy (pPacket, EnetDmaRx[index].pBuffer, frameSize);
	}

	index++;
	if (index == RX_BUFFER_COUNT)
		index = 0;
	RXCONSUMEINDEX = index;

	return frameSize;
}

// ============================================================================

void tapdev_send (void* pPacket, unsigned int size)
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


