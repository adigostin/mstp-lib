
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_bridge.h"
#include "stp_log.h"
#include <stddef.h>

#ifdef __GNUC__
	// For GCC older than 8.x: disable the warning for accessing a field of a non-POD NULL object
	#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

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
// 14.2.9 in 802.1Q-2018
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

static bool ContainsVersion1LengthField (size_t bpduSize)
{
	return bpduSize >= offsetof(MSTP_BPDU, Version1Length) + sizeof(MSTP_BPDU::Version1Length);
}

unsigned char GetVersion1Length (const unsigned char* bpdu)
{
	return reinterpret_cast<const MSTP_BPDU*>(bpdu)->Version1Length;
}

static bool ContainsVersion3LengthField (size_t bpduSize)
{
	return bpduSize >= offsetof(MSTP_BPDU, Version3Length) + sizeof(MSTP_BPDU::Version3Length);
}

static bool Version3LengthIsIntegralNumberOfMstiConfigMsgs (const unsigned char* bpdu)
{
	size_t version3Length = reinterpret_cast<const MSTP_BPDU*>(bpdu)->Version3Length;
	size_t version3Offset = offsetof (struct MSTP_BPDU, mstConfigId);
	size_t version3CistLength = sizeof(MSTP_BPDU) - version3Offset;
	size_t mstiLength = version3Length - version3CistLength;
	return ((mstiLength % sizeof(MSTI_CONFIG_MESSAGE)) == 0) && (mstiLength / sizeof(MSTI_CONFIG_MESSAGE) <= 64);
}

static bool IsWellFormedSptBpdu (const unsigned char* bpdu, size_t bpduSize)
{
	//   4) Is a well-formed an SPT BPDU, i.e., contains
	//        i) At least 6 octets following the octets specified by the Version 3 length,
	//        ii) A Version 4 length of 55 or greater,
	//        iii) An MCID Format Selector of 1,
	size_t version3Length = reinterpret_cast<const MSTP_BPDU*>(bpdu)->Version3Length;
	size_t version3Offset = offsetof (struct MSTP_BPDU, mstConfigId);
	if (bpduSize < version3Offset + version3Length + 6)
		return false;

	size_t version4Length = reinterpret_cast<const INV_UINT2*>(bpdu + version3Offset + version3Length)->GetValue();
	if (version4Length < 55)
		return false;

	size_t version4Offset = version3Offset + version3Length + 2;
	const STP_MST_CONFIG_ID* auxiliaryMcid = reinterpret_cast<const STP_MST_CONFIG_ID*>(bpdu + version4Offset);
	return auxiliaryMcid->ConfigurationIdentifierFormatSelector == 1;
}

// 14.5 in 802.1Q-2018: Validation of received BPDUs
//
// The receiving protocol entity shall examine Octets 1 and 2 (conveying the Protocol Identifier), Octet 3
// (conveying the Protocol Version Identifier encoded as a number), Octet 4 (conveying the BPDU Type) and
// the total length of the received BPDU (including the preceding fields, but none prior to the Protocol
// Identifier) to determine the further processing required as follows:
enum VALIDATED_BPDU_TYPE STP_GetValidatedBpduType (enum STP_VERSION bridgeStpVersion, const unsigned char* bpdu, size_t bpduSize)
{
	if (bpduSize < sizeof(BPDU_HEADER))
		return VALIDATED_BPDU_TYPE_UNKNOWN;

	const BPDU_HEADER* bpduHeader = reinterpret_cast<const BPDU_HEADER*>(bpdu);
	if (bpduHeader->protocolId != 0)
		return VALIDATED_BPDU_TYPE_UNKNOWN;

	// a) If the Protocol Identifier is 0000 0000 0000 0000, the BPDU Type is 0000 0000, and the BPDU
	// contains 35 or more octets, it shall be decoded as an STP Configuration BPDU.
	if ((bpduHeader->bpduType == 0) && (bpduSize >= 35))
		return VALIDATED_BPDU_TYPE_STP_CONFIG;

	// b) If the Protocol Identifier is 0000 0000 0000 0000, the BPDU Type is 1000 0000 (where bit 8 is
	// shown at the left of the sequence), and the BPDU contains 4 or more octets, it shall be decoded as an
	//STP TCN BPDU (9.3.2 of IEEE Std 802.1D-2004 [B17]).
	if ((bpduHeader->bpduType == 0x80) && (bpduSize >= 4))
		return VALIDATED_BPDU_TYPE_STP_TCN;

	// c) If the Protocol Identifier is 0000 0000 0000 0000, the Protocol Version Identifier is 2, and the BPDU
	// Type is 0000 0010 (where bit 8 is shown at the left of the sequence), and the BPDU contains 36 or
	// more octets, it shall be decoded as an RST BPDU.
	if ((bpduHeader->protocolVersionId == 2) && (bpduHeader->bpduType == 2) && (bpduSize >= 36))
		return VALIDATED_BPDU_TYPE_RST;

	// Note AG:
	// When we receive a BPDU from a bridge running MSTP or SPT (BPDU's Protocol Version Identifier >= 3) and
	// we are running RSTP or LegacySTP (our ForceProtocolVersion is <= 2), none of the conditions in 14.5 is
	// satisfied and if we stick to the wording of the standard we'd have to discard the BPDU. This behavior,
	// however, would make us a malfunctioning bridge. The behavior is also against the principle described in
	// the very next paragraph "14.6 Validation and interoperability". Here I choose to depart from the
	// standard and not discard the BPDU. I do this by inserting the following condition:
	if ((bpduHeader->protocolVersionId >= 3) && (bpduHeader->bpduType == 2) && (bpduSize >= 36) && (bridgeStpVersion <= STP_VERSION_RSTP))
		return VALIDATED_BPDU_TYPE_RST;

	// d) If the Protocol Identifier is 0000 0000 0000 0000, the Protocol Version Identifier is 3 or greater, the
	// BPDU Type is 0000 0010, the Bridge is configured as an MST Bridge or an SPT Bridge or
	// according to a future revision of this standard that intends to provide interoperability with prior
	// revisions, and the BPDU
	//   1) Contains 35 or more but less than 103 octets, or
	//   2) Contains a Version 1 Length that is not 0, or
	//   3) Contains a Version 3 length that does not represent an integral number, from 0 to 64 inclusive,
	//      of MSTI Configuration Messages,
	// it shall be decoded as an RST BPDU.
	//
	// Note AG: I am not OK with this condition either, specifically with "but less than 103 octets".
	// This condition would cause us to treat an MSTP BPDU without MSTI messages (102 bytes long)
	// received from the same region (same MCID as ours) as though it were an RSPT BPDU, so we'd
	// be seeing it - wrongly - as coming from a different region (fromSameRegion would return false).
	// Moreover, such a BPDU would satify both the d) and e) conditions, and without an evaluation
	// order imposed by the standard the limit of 103 octets would leave room for interpretation to
	// implementors (bad). Also here I choose to depart from the standard and use 102 instead of 103;
	// this way conditions d) and e) become mutually exclusive (good i.m.o.)
	if ((bpduHeader->protocolVersionId >= 3) && (bpduHeader->bpduType == 2) && (bridgeStpVersion >= STP_VERSION_MSTP)
		&& (((bpduSize >= 35) && (bpduSize < 102))
			|| (ContainsVersion1LengthField(bpduSize) && (GetVersion1Length(bpdu) != 0))
			|| (ContainsVersion3LengthField(bpduSize) && !Version3LengthIsIntegralNumberOfMstiConfigMsgs(bpdu))))
	{
		return VALIDATED_BPDU_TYPE_RST;
	}

	// e) If the Protocol Identifier is 0000 0000 0000 0000, the Protocol Version Identifier is 3 or greater and
	// the Bridge is configured as an MST Bridge or the Protocol Version Identifier is 3 and the Bridge is
	// configured as an SPT Bridge or according to a future revision of this standard that intends to provide
	// interoperability with prior revisions, the BPDU Type is 0000 0010, and the BPDU
	//   1) Contains 102 or more octets, and
	//   2) Contains a Version 1 Length of 0, and
	//   3) Contains a Version 3 length representing an integral number, from 0 to 64 inclusive, of MSTI
	//      Configuration Messages,
	// it shall be decoded as an MST BPDU.
	if ((((bpduHeader->protocolVersionId >= 3) && (bridgeStpVersion == STP_VERSION_MSTP))
		|| ((bpduHeader->protocolVersionId == 3) && (bridgeStpVersion  > STP_VERSION_MSTP)))
		&& (bpduHeader->bpduType == 2)
		&& (bpduSize >= 102)
		&& ((ContainsVersion1LengthField(bpduSize)) && (GetVersion1Length(bpdu) == 0))
		&& ((ContainsVersion3LengthField(bpduSize)) && Version3LengthIsIntegralNumberOfMstiConfigMsgs(bpdu)))
	{
		return VALIDATED_BPDU_TYPE_MST;
	}

	// f) If the Protocol Identifier is 0000 0000 0000 0000, the Protocol Version Identifier is 3 or greater, the
	// BPDU Type is 0000 0010, the Bridge is configured as an SPT Bridge or according to a future
	// revision of this standard that intends to provide interoperability with prior revisions, and the BPDU
	//   1) Contains 102 or more octets, and
	//   2) Contains a Version 1 Length of 0, and
	//   3) Contains a Version 3 length representing an integral number, from 0 to 64 inclusive, of MSTI
	//      Configuration Messages, and
	//   4) Is not a well-formed an SPT BPDU, i.e.,
	//        i) Contains less than 6 octets following the octets specified by the Version 3 length,
	//        ii) Has a Version 4 length that is less than 55,
	//        iii) Does not contain an MCID Format Selector of 1,
	// it shall be decoded as an MST BPDU.
	if ((bpduHeader->protocolVersionId >= 3) && (bpduHeader->bpduType == 2) && (bridgeStpVersion > STP_VERSION_MSTP)
		&& (bpduSize >= 102)
		&& (ContainsVersion1LengthField(bpduSize) && (GetVersion1Length(bpdu) == 0))
		&& (ContainsVersion3LengthField(bpduSize) && Version3LengthIsIntegralNumberOfMstiConfigMsgs(bpdu))
		&& !IsWellFormedSptBpdu(bpdu, bpduSize))
	{
		return VALIDATED_BPDU_TYPE_MST;
	}

	// g) If the Protocol Identifier is 0000 0000 0000 0000, the Protocol Version Identifier is 4 or greater, the
	// BPDU Type is 0000 0010, the Bridge is configured as an MST Bridge or an SPT Bridge or
	// according to a future revision of this standard that intends to provide interoperability with prior
	// revisions, and the BPDU
	//   1) Contains 106 or more octets, and
	//   2) Contains a Version 1 Length of 0, and
	//   3) Contains a Version 3 length representing an integral number, from 0 to 64 inclusive, of MSTI
	//      Configuration Messages, and
	//   4) Is a well-formed an SPT BPDU, i.e., contains
	//        i) At least 6 octets following the octets specified by the Version 3 length,
	//        ii) A Version 4 length of 55 or greater,
	//        iii) An MCID Format Selector of 1,
	// it shall be decoded as an SPT BPDU.
	if ((bpduHeader->protocolVersionId >= 4) && (bpduHeader->bpduType == 2) && (bridgeStpVersion >= STP_VERSION_MSTP)
		&& (bpduSize >= 106)
		&& (ContainsVersion1LengthField(bpduSize) && (GetVersion1Length(bpdu) == 0))
		&& (ContainsVersion3LengthField(bpduSize) && Version3LengthIsIntegralNumberOfMstiConfigMsgs(bpdu))
		&& IsWellFormedSptBpdu(bpdu, bpduSize))
	{
		return VALIDATED_BPDU_TYPE_SPT;
	}

	// h) Otherwise, the BPDU shall be discarded and not processed.
	return VALIDATED_BPDU_TYPE_UNKNOWN;
}

// ============================================================================

#if STP_USE_LOG
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
#endif
