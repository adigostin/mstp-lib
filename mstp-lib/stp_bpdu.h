
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_BPDU_H
#define MSTP_LIB_BPDU_H

#include "stp_base_types.h"
#include "stp.h"

// 14.2.1
enum BPDU_PORT_ROLE
{
	BPDU_PORT_ROLE_MASTER     = 0,
	BPDU_PORT_ROLE_ALT_BACKUP = 1,
	BPDU_PORT_ROLE_ROOT       = 2,
	BPDU_PORT_ROLE_DESIGNATED = 3,
};

// 14.6
bool           GetBpduFlagTc         (unsigned char bpduFlags);
bool           GetBpduFlagProposal   (unsigned char bpduFlags);
BPDU_PORT_ROLE GetBpduFlagPortRole   (unsigned char bpduFlags);
bool           GetBpduFlagLearning   (unsigned char bpduFlags);
bool           GetBpduFlagForwarding (unsigned char bpduFlags);
bool           GetBpduFlagAgreement  (unsigned char bpduFlags);
bool           GetBpduFlagTcAck      (unsigned char bpduFlags);
bool           GetBpduFlagMaster     (unsigned char bpduFlags);


struct MSTI_CONFIG_MESSAGE
{
	unsigned char flags;
	BRIDGE_ID     RegionalRootId;
	INV_UINT4     InternalRootPathCost;

	// Bits 5 through 8 of Octet 14 convey the value of the Bridge Identifier Priority for this MSTI.
	// Bits 1 through 4 of Octet 14 shall be transmitted as 0, and ignored on receipt.
	unsigned char BridgePriority;

	// Bits 5 through 8 of Octet 15 convey the value of the Port Identifier Priority for this MSTI.
	// Bits 1 through 4 of Octet 15 shall be transmitted as 0, and ignored on receipt.
	unsigned char PortPriority;

	unsigned char RemainingHops;

	void Dump (STP_BRIDGE* bridge, int port, int tree) const;
};

// ============================================================================

struct BPDU_HEADER
{
	INV_UINT2 protocolId;
	unsigned char protocolVersionId;
	unsigned char bpduType;
};

// ============================================================================

// ============================================================================

// The library uses this structure for STP Config BPDUs, RSTP BPDUs and MSTP BPDUs.
//  - STP Config BPDUs use members up to and including ForwardDelay.
//  - RSTP BPDUs use members up to and including Version1Length.
//  - MSTP BPDUs use members up to and including mstiConfigMessages (the array length can be zero or more)
struct MSTP_BPDU : public BPDU_HEADER
{
	unsigned char cistFlags;			// octet 5

	BRIDGE_ID	cistRootId;				// octets 6 to 13
	INV_UINT4	cistExternalPathCost;	// octets 14 to 17
	BRIDGE_ID	cistRegionalRootId;		// octets 18 to 25
	PORT_ID		cistPortId;				// octets 26 to 27

	INV_UINT2	MessageAge;		// octets 28 to 29
	INV_UINT2	MaxAge;			// octets 30 to 31
	INV_UINT2	HelloTime;		// octets 32 to 33
	INV_UINT2	ForwardDelay;	// octets 34 to 35

	unsigned char Version1Length;
	INV_UINT2 Version3Length;

	STP_MST_CONFIG_ID	mstConfigId;

	INV_UINT4		cistInternalRootPathCost;
	BRIDGE_ID		cistBridgeId;

	unsigned char	cistRemainingHops;

	// MSTI_CONFIG_MESSAGE mstiConfigMessages [0];
};

// ============================================================================

enum VALIDATED_BPDU_TYPE
{
	VALIDATED_BPDU_TYPE_UNKNOWN,
	VALIDATED_BPDU_TYPE_STP_CONFIG,
	VALIDATED_BPDU_TYPE_STP_TCN,
	VALIDATED_BPDU_TYPE_RST,
	VALIDATED_BPDU_TYPE_MST
};

enum VALIDATED_BPDU_TYPE STP_GetValidatedBpduType (const unsigned char* _bpdu, unsigned int bpduSize);

BPDU_PORT_ROLE GetBpduPortRole (STP_PORT_ROLE role);

void DumpMstpBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu);
void DumpRstpBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu);
void DumpConfigBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu);

// ============================================================================

#endif
