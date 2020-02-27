
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_BPDU_H
#define MSTP_LIB_BPDU_H

#include "stp_base_types.h"
#include "../stp.h"

// 14.2.9 in 802.1Q-2018
enum BPDU_PORT_ROLE
{
	BPDU_PORT_ROLE_MASTER     = 0,
	BPDU_PORT_ROLE_ALT_BACKUP = 1,
	BPDU_PORT_ROLE_ROOT       = 2,
	BPDU_PORT_ROLE_DESIGNATED = 3,
};

inline bool           GetBpduFlagTc         (unsigned char bpduFlags) { return (bpduFlags & 1) != 0; } // 14.4.a) in 802.1Q-2018
inline bool           GetBpduFlagProposal   (unsigned char bpduFlags) { return (bpduFlags & 2) != 0; } // 14.4.b) in 802.1Q-2018
inline BPDU_PORT_ROLE GetBpduFlagPortRole   (unsigned char bpduFlags) { return (BPDU_PORT_ROLE) ((bpduFlags >> 2) & 3); } // 14.4.c) in 802.1Q-2018
inline bool           GetBpduFlagLearning   (unsigned char bpduFlags) { return (bpduFlags & 0x10) != 0; } // 14.4.d) in 802.1Q-2018
inline bool           GetBpduFlagForwarding (unsigned char bpduFlags) { return (bpduFlags & 0x20) != 0; } // 14.4.e) in 802.1Q-2018
inline bool           GetBpduFlagAgreement  (unsigned char bpduFlags) { return (bpduFlags & 0x40) != 0; } // 14.4.f) in 802.1Q-2018
inline bool           GetBpduFlagTcAck      (unsigned char bpduFlags) { return (bpduFlags & 0x80) != 0; } // 14.4.g) in 802.1Q-2018
inline bool           GetBpduFlagMaster     (unsigned char bpduFlags) { return (bpduFlags & 0x80) != 0; } // 14.4.1.a) in 802.1Q-2018

// 14.4.1 in 802.1Q-2018
struct MSTI_CONFIG_MESSAGE
{
	unsigned char flags; // a)
	BRIDGE_ID     RegionalRootId; // b)
	uint32_nbo    InternalRootPathCost; // c)

	// d) Bits 5 through 8 of Octet 14 convey the value of the Bridge Identifier Priority for this MSTI.
	// Bits 1 through 4 of Octet 14 shall be transmitted as 0, and ignored on receipt.
	unsigned char BridgePriority;

	// e) Bits 5 through 8 of Octet 15 convey the value of the Port Identifier Priority for this MSTI.
	// Bits 1 through 4 of Octet 15 shall be transmitted as 0, and ignored on receipt.
	unsigned char PortPriority;

	unsigned char RemainingHops; // f)

	void Dump (STP_BRIDGE* bridge, int port, int tree) const;
};

// ============================================================================
// 14.1.2 in 802.1Q-2018
struct BPDU_HEADER
{
	uint16_nbo    protocolId;
	unsigned char protocolVersionId;
	unsigned char bpduType;
};

// ============================================================================
// Figure 14-1 in 802.1Q-2018
// The library uses this structure for STP Config BPDUs, RSTP BPDUs and MSTP BPDUs.
//  - STP Config BPDUs use members up to and including ForwardDelay.
//  - RSTP BPDUs use members up to and including Version1Length.
//  - MSTP BPDUs use members up to and including mstiConfigMessages (the array length can be zero or more)
struct MSTP_BPDU : BPDU_HEADER
{
	unsigned char cistFlags;			// octet 5

	BRIDGE_ID	cistRootId;				// octets 6 to 13
	uint32_nbo	cistExternalPathCost;	// octets 14 to 17
	BRIDGE_ID	cistRegionalRootId;		// octets 18 to 25 - called Bridge Identifier pre-MSTP, referred to also as Designated Bridge
	PORT_ID		cistPortId;				// octets 26 to 27 - called Port Identifier pre-MSTP, referred to also as Designated Port

	uint16_nbo	MessageAge;		// octets 28 to 29
	uint16_nbo	MaxAge;			// octets 30 to 31
	uint16_nbo	HelloTime;		// octets 32 to 33
	uint16_nbo	ForwardDelay;	// octets 34 to 35

	unsigned char Version1Length;
	uint16_nbo    Version3Length;

	STP_MST_CONFIG_ID	mstConfigId;

	uint32_nbo		cistInternalRootPathCost;
	BRIDGE_ID		cistBridgeId;

	unsigned char	cistRemainingHops;

	// MSTI_CONFIG_MESSAGE mstiConfigMessages [0];
};

// ============================================================================
// 14.5 in 802.1Q-2018
enum VALIDATED_BPDU_TYPE
{
	VALIDATED_BPDU_TYPE_UNKNOWN,
	VALIDATED_BPDU_TYPE_STP_CONFIG,
	VALIDATED_BPDU_TYPE_STP_TCN,
	VALIDATED_BPDU_TYPE_RST,
	VALIDATED_BPDU_TYPE_MST,
	VALIDATED_BPDU_TYPE_SPT,
};

enum VALIDATED_BPDU_TYPE STP_GetValidatedBpduType (enum STP_VERSION bridgeStpVersion, const unsigned char* bpdu, size_t bpduSize);

BPDU_PORT_ROLE GetBpduPortRole (STP_PORT_ROLE role);

#if STP_USE_LOG
void DumpMstpBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu);
void DumpRstpBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu);
void DumpConfigBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu);
#endif

#endif
