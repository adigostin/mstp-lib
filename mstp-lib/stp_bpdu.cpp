
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#define _CRT_SECURE_NO_WARNINGS

#include "stp_bridge.h"
#include "stp_log.h"
#include <stddef.h>
#include <string.h>

#ifdef __GNUC__
	// disable the warning for accessing a field of a non-POD NULL object
	#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

// ============================================================================

const char* GetBpduPortRoleName (unsigned int bpduPortRole)
{
	if (bpduPortRole == 0)
		return "Unknown";
	else if (bpduPortRole == 1)
		return "Alt/Backup";
	else if (bpduPortRole == 2)
		return "Root";
	else if (bpduPortRole == 3)
		return "Designated";
	else
	{
		assert (false);
		return NULL;
	}
}

// ============================================================================

BPDU_PORT_ROLE GetBpduPortRole (STP_PORT_ROLE role)
{
	if (role == STP_PORT_ROLE_MASTER)
	{
		return BPDU_PORT_ROLE_MASTER;
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) || (role == STP_PORT_ROLE_BACKUP))
	{
		return BPDU_PORT_ROLE_ALT_BACKUP;
	}
	else if (role == STP_PORT_ROLE_ROOT)
	{
		return BPDU_PORT_ROLE_ROOT;
	}
	else if (role == STP_PORT_ROLE_DESIGNATED)
	{
		return BPDU_PORT_ROLE_DESIGNATED;
	}
	else
	{
		assert (false);
		return (BPDU_PORT_ROLE) 0;
	}
}

// ============================================================================

enum VALIDATED_BPDU_TYPE STP_GetValidatedBpduType (const unsigned char* _bpdu, unsigned int bpduSize)
{
	BPDU_HEADER* bpduHeader = (BPDU_HEADER*) _bpdu;
	if (bpduHeader->protocolId.GetValue () != 0)
		return VALIDATED_BPDU_TYPE_UNKNOWN;

	// 14.4 in 802.1Q-2011
	if ((bpduHeader->bpduType == 0) && (bpduSize >= 35))
	{
		// a) - STP Configuration BPDU

		// TODO: do additonal checks as specified in 9.3.4.a) in 802.1D-2004.

		return VALIDATED_BPDU_TYPE_STP_CONFIG;
	}
	else if ((bpduHeader->bpduType == 0x80) && (bpduSize >= 4))
	{
		// b) - STP TCN BPDU
		return VALIDATED_BPDU_TYPE_STP_TCN;
	}
	else if ((bpduHeader->protocolVersionId == 2) && (bpduHeader->bpduType == 2) && (bpduSize >= 36))
	{
		// c) - RST BPDU
		return VALIDATED_BPDU_TYPE_RST;
	}
	else if ((bpduHeader->protocolVersionId >= 3) && (bpduHeader->bpduType == 2))
	{
		MSTP_BPDU* mstpBpdu = (MSTP_BPDU*) _bpdu;

		// Conditions d) / 1), 2), 3) don't make much sense or-ed, as they appear in the standard. Truncated
		// or malformed BPDUs would pass as valid RSTP BPDUs according to these conditions.
		// Let's just implement e) and discard non-matching BPDUs.
		if (bpduSize >= 102)
		{
			unsigned short version3Length = mstpBpdu->Version3Length.GetValue ();
			unsigned short version3Offset = (unsigned short) offsetof (struct MSTP_BPDU, mstConfigId);
			unsigned short version3CistLength = (unsigned short) sizeof (MSTP_BPDU) - version3Offset;
			unsigned short mstiLength = version3Length - version3CistLength;

			if ((mstpBpdu->Version1Length == 0) && ((mstiLength % 16) == 0) && (bpduSize == sizeof (MSTP_BPDU) + mstiLength))
			{
				// MST BPDU
				return VALIDATED_BPDU_TYPE_MST;
			}
		}

		return VALIDATED_BPDU_TYPE_UNKNOWN;
	}
	else
	{
		return VALIDATED_BPDU_TYPE_UNKNOWN;
	}
}

// ============================================================================

void DumpMstpBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu)
{
	LOG (bridge, port, tree, "Flags: TC={D}, Proposal={D}, PortRole={S}, Learning={D}, Forwarding={D}, Agreement={D}\r\n",
			(int) GetBpduFlagTc (bpdu->cistFlags),
			(int) GetBpduFlagProposal (bpdu->cistFlags),
			GetBpduPortRoleName (GetBpduFlagPortRole (bpdu->cistFlags)),
			(int) GetBpduFlagLearning (bpdu->cistFlags),
			(int) GetBpduFlagForwarding (bpdu->cistFlags),
			(int) GetBpduFlagAgreement (bpdu->cistFlags));
	LOG (bridge, port, tree, "CIST Root ID                 : {BID}\r\n", &bpdu->cistRootId);
	LOG (bridge, port, tree, "CIST External Path Cost      : {D7}\r\n",  (int) bpdu->cistExternalPathCost.GetValue ());
	LOG (bridge, port, tree, "CIST Regional Root ID        : {BID}\r\n", &bpdu->cistRegionalRootId);
	LOG (bridge, port, tree, "CIST Internal Root Path Cost : {D7}\r\n",  (int) bpdu->cistInternalRootPathCost.GetValue ());
	LOG (bridge, port, tree, "CIST Bridge ID               : {BID}\r\n", &bpdu->cistBridgeId);
	LOG (bridge, port, tree, "CIST Port ID                 : {PID}\r\n", &bpdu->cistPortId);
	LOG (bridge, port, tree, "CIST MessageAge={D}, MaxAge={D}, HelloTime={D}, ForwardDelay={D}, remainingHops={D}\r\n",
		 bpdu->MessageAge.GetValue () / 256,
		 bpdu->MaxAge.GetValue () / 256,
		 bpdu->HelloTime.GetValue () / 256,
		 bpdu->ForwardDelay.GetValue () / 256,
		 (int) bpdu->cistRemainingHops);

	bpdu->mstConfigId.Dump (bridge, port, tree);

	unsigned short v3Length = bpdu->Version3Length.GetValue ();
	unsigned short headerRemainder = (unsigned short) sizeof (MSTP_BPDU) - (unsigned short) offsetof (struct MSTP_BPDU, mstConfigId);
	assert (v3Length >= headerRemainder);
	assert (((v3Length - headerRemainder) % 16) == 0);
	unsigned short mstiCount = (v3Length - headerRemainder) / 16;

	MSTI_CONFIG_MESSAGE* mstis = (MSTI_CONFIG_MESSAGE*) &bpdu [1];
	for (int mstiIndex = 0; mstiIndex < mstiCount; mstiIndex++)
	{
		LOG (bridge, port, tree, "MSTI #{D}\r\n", mstiIndex + 1);
		LOG_INDENT (bridge);
		mstis [mstiIndex].Dump (bridge, port, tree);
		LOG_UNINDENT (bridge);
	}
}

// ============================================================================

void DumpRstpBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu)
{
	LOG (bridge, port, tree, "Flags: TC={D}, Proposal={D}, PortRole={S}, Learning={D}, Forwarding={D}, Agreement={D}\r\n",
			(int) GetBpduFlagTc (bpdu->cistFlags),
			(int) GetBpduFlagProposal (bpdu->cistFlags),
			GetBpduPortRoleName (GetBpduFlagPortRole (bpdu->cistFlags)),
			(int) GetBpduFlagLearning (bpdu->cistFlags),
			(int) GetBpduFlagForwarding (bpdu->cistFlags),
			(int) GetBpduFlagAgreement (bpdu->cistFlags));
	LOG (bridge, port, tree, "  Root ID        : {BID}\r\n", &bpdu->cistRootId);
	LOG (bridge, port, tree, "  Root Path Cost : {D7}\r\n",   (int) bpdu->cistExternalPathCost.GetValue ());
	LOG (bridge, port, tree, "  Bridge ID      : {BID}\r\n", &bpdu->cistRegionalRootId);
	LOG (bridge, port, tree, "  Port ID        : {PID}\r\n", &bpdu->cistPortId);
	LOG (bridge, port, tree, "  MessageAge={D}, MaxAge={D}, HelloTime={D}, ForwardDelay={D}\r\n",
		 bpdu->MessageAge.GetValue () / 256,
		 bpdu->MaxAge.GetValue () / 256,
		 bpdu->HelloTime.GetValue () / 256,
		 bpdu->ForwardDelay.GetValue () / 256);
}

void DumpConfigBpdu (STP_BRIDGE* bridge, int port, int tree, const MSTP_BPDU* bpdu)
{
	LOG (bridge, port, tree, "Flags: TC={D}, TCAck={D}\r\n",
			(int) GetBpduFlagTc    (bpdu->cistFlags),
			(int) GetBpduFlagTcAck (bpdu->cistFlags));
	LOG (bridge, port, tree, "  Root ID        : {BID}\r\n", &bpdu->cistRootId);
	LOG (bridge, port, tree, "  Root Path Cost : {D7}\r\n",   (int) bpdu->cistExternalPathCost.GetValue ());
	LOG (bridge, port, tree, "  Bridge ID      : {BID}\r\n", &bpdu->cistRegionalRootId);
	LOG (bridge, port, tree, "  Port ID        : {PID}\r\n", &bpdu->cistPortId);
	LOG (bridge, port, tree, "  MessageAge={D}, MaxAge={D}, HelloTime={D}, ForwardDelay={D}\r\n",
		 bpdu->MessageAge.GetValue () / 256,
		 bpdu->MaxAge.GetValue () / 256,
		 bpdu->HelloTime.GetValue () / 256,
		 bpdu->ForwardDelay.GetValue () / 256);
}

// ============================================================================

void MSTI_CONFIG_MESSAGE::Dump (STP_BRIDGE* bridge, int port, int tree) const
{
	LOG (bridge, port, tree, "Flags: TC={D}, Proposal={D}, PortRole={S}, Learning={D}, Forwarding={D}, Agreement={D}, Master={D}\r\n",
			(int) GetBpduFlagTc (flags),
			(int) GetBpduFlagProposal (flags),
			GetBpduPortRoleName (GetBpduFlagPortRole (flags)),
			(int) GetBpduFlagLearning (flags),
			(int) GetBpduFlagForwarding (flags),
			(int) GetBpduFlagAgreement (flags),
			(int) GetBpduFlagMaster (flags));
	LOG (bridge, port, tree, "RegionalRootId       : {BID}\r\n", &RegionalRootId);
	LOG (bridge, port, tree, "InternalRootPathCost : {D}\r\n", InternalRootPathCost.GetValue ());
	LOG (bridge, port, tree, "BridgePriority       : 0x{X2}\r\n", BridgePriority);
	LOG (bridge, port, tree, "PortPriority         : 0x{X2}\r\n", PortPriority);
	LOG (bridge, port, tree, "RemainingHops        : {D}\r\n", RemainingHops);
}

// ============================================================================
// 14.6.a)
bool GetBpduFlagTc (unsigned char bpduFlags)
{
	return (bpduFlags & 1) != 0;
}

// 14.6.b)
bool GetBpduFlagProposal (unsigned char bpduFlags)
{
	return (bpduFlags & 2) != 0;
}

// 14.6.c)
BPDU_PORT_ROLE GetBpduFlagPortRole (unsigned char bpduFlags)
{
	return (BPDU_PORT_ROLE) ((bpduFlags >> 2) & 3);
}

// 14.6.d)
bool GetBpduFlagLearning (unsigned char bpduFlags)
{
	return (bpduFlags & 0x10) != 0;
}

// 14.6.e)
bool GetBpduFlagForwarding (unsigned char bpduFlags)
{
	return (bpduFlags & 0x20) != 0;
}

// 14.6.f)
bool GetBpduFlagAgreement (unsigned char bpduFlags)
{
	return (bpduFlags & 0x40) != 0;
}

// 14.6.g)
bool GetBpduFlagTcAck (unsigned char bpduFlags)
{
	return (bpduFlags & 0x80) != 0;
}

// 14.6.1.a)
bool GetBpduFlagMaster (unsigned char bpduFlags)
{
	return (bpduFlags & 0x80) != 0;
}

// ============================================================================
