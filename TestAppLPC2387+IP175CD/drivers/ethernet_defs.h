
#pragma once


// Transmit descriptor control word
// See LPC23xx User Manual Ch16, Table 212
union EnetTxCtrl_t
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
};

// Transmit descriptor
// See LPC23xx User Manual Ch16, Table 211
struct EnetDmaTxDesc_t
{
	unsigned char* pBuffer;
	EnetTxCtrl_t EnetTxCtrl;
};

// Transmit status
// See LPC23xx User Manual Ch16, Table 211
union EnetDmaTxStatus_t
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
};

// Receive descriptor control word
// See LPC23xx User Manual Ch16, Table 207
union EnetRxCtrl_t
{
	uint32_t data;
	struct
	{
		uint32_t Size : 11;
		uint32_t      : 20;
		uint32_t Intr :  1;
	};
};

// Receive descriptor
// See LPC23xx User Manual Ch16, Table 206
struct EnetDmaRxDesc_t
{
	unsigned char* pBuffer;
	EnetRxCtrl_t EnetRxCtrl;
};

// Receive status
// See LPC23xx User Manual Ch16, Table 208
union EnetDmaRxStatus_t
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
};
