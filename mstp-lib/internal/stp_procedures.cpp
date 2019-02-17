
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include "stp_port.h"
#include "stp_log.h"
#include <assert.h>
#include <stddef.h>

// ============================================================================
// 13.27.a - 13.27.1
// Returns TRUE if, for a given port and tree (CIST, or MSTI), either
// a) The procedure's parameter newInfoIs is Received, and infoIs is Received and the msgPriority vector
//    is better than or the same as (13.9) the portPriority vector; or,
// b) The procedure's parameter newInfoIs is Mine, and infoIs is Mine and the designatedPriority vector is
//    better than or the same as (13.9) the portPriority vector.
// Returns False otherwise.
bool betterorsameInfo (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, INFO_IS newInfoIs)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	if ((newInfoIs == INFO_IS_RECEIVED) && (tree->infoIs == INFO_IS_RECEIVED) && (tree->msgPriority.IsBetterThanOrSameAs (tree->portPriority)))
		return true;

	if ((newInfoIs == INFO_IS_MINE) && (tree->infoIs == INFO_IS_MINE) && (tree->designatedPriority.IsBetterThanOrSameAs (tree->portPriority)))
		return true;

	return false;
}

// ============================================================================
// 13.27.b) - 13.27.2
// Clears rcvdMsg for the CIST and all MSTIs, for this port.
void clearAllRcvdMsgs (STP_BRIDGE* bridge, PortIndex givenPort)
{
	for (unsigned int treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
		bridge->ports [givenPort]->trees [treeIndex]->rcvdMsg = false;
}

// ============================================================================
// 13.27.c) - 13.27.3
// Clears reselect for the tree (the CIST or a given MSTI) for all ports of the bridge.
void clearReselectTree (STP_BRIDGE* bridge, TreeIndex givenTree)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		bridge->ports [portIndex]->trees [givenTree]->reselect = false;
}

// ============================================================================
// 13.27.d - 13.27.4
void disableForwarding (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, unsigned int timestamp)
{
	FLUSH_LOG (bridge);
	bridge->callbacks.enableForwarding (bridge, givenPort, givenTree, false, timestamp);
}

// ============================================================================
// 13.27.e - 13.27.5
void disableLearning (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, unsigned int timestamp)
{
	FLUSH_LOG (bridge);
	bridge->callbacks.enableLearning (bridge, givenPort, givenTree, false, timestamp);
}

// ============================================================================
// 13.27.f - 13.27.6
void enableForwarding (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, unsigned int timestamp)
{
	FLUSH_LOG (bridge);

	bridge->callbacks.enableForwarding (bridge, givenPort, givenTree, true, timestamp);
}

// ============================================================================
// 13.27.g - 13.27.7
void enableLearning (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, unsigned int timestamp)
{
	FLUSH_LOG (bridge);

	bridge->callbacks.enableLearning (bridge, givenPort, givenTree, true, timestamp);
}

// ============================================================================
// 13.27.h - 13.27.8
// Returns TRUE if rcvdRSTP is TRUE, and the received BPDU conveys a MST Configuration Identifier that
// matches that held for the bridge. Returns FALSE otherwise.
bool fromSameRegion (STP_BRIDGE* bridge, PortIndex givenPort)
{
	PORT* port = bridge->ports [givenPort];

	assert (bridge->receivedBpduContent != nullptr);

	// Note AG: I added the condition "&& ForceProtocolVersion >= MSTP"
	// (if we're running STP or RSTP, we shouldn't be looking at our MST Config ID!)

	bool result = port->rcvdRSTP
		&& (bridge->receivedBpduType == VALIDATED_BPDU_TYPE_MST)
		&& (bridge->ForceProtocolVersion >= STP_VERSION_MSTP)
		&& (bridge->receivedBpduContent->mstConfigId == bridge->MstConfigId);

	return result;
}

// ============================================================================
// 13.27.i - 13.27.9
// If the value of tcDetected is zero and sendRSTP is TRUE, this procedure sets the value of tcDetected to
// HelloTime plus one second. The value of HelloTime is taken from the CIST's portTimes parameter (13.25.34)
// for this port.
//
// If the value of tcDetected is zero and sendRSTP is FALSE, this procedure sets the value of tcDetected to the
// sum of the Max Age and Forward Delay components of rootTimes.
//
// Otherwise the procedure takes no action.
void newTcDetected (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	if ((tree->tcDetected == 0) && port->sendRSTP)
		tree->tcDetected = port->trees [CIST_INDEX]->portTimes.HelloTime + 1;

	if ((tree->tcDetected == 0) && (port->sendRSTP == false))
	{
		tree->tcDetected = bridge->trees [givenTree]->rootTimes.MaxAge + bridge->trees [givenTree]->rootTimes.ForwardDelay;
	}
}

// ============================================================================
// 13.27.j) - 13.27.10
// If the value of tcWhile is zero and sendRSTP is TRUE, this procedure sets the value of tcWhile to HelloTime
// plus one second and sets either newInfo TRUE for the CIST or newInfoMsti TRUE for a given MSTI. The
// value of HelloTime is taken from the CIST's portTimes parameter (13.25.34) for this port.
//
// If the value of tcWhile is zero and sendRSTP is FALSE, this procedure sets the value of tcWhile to the sum
// of the Max Age and Forward Delay components of rootTimes and does not change the value of either
// newInfo or newInfoMsti.
//
// Otherwise the procedure takes no action.
void newTcWhile (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, unsigned int timestamp)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	if ((portTree->tcWhile == 0) && (port->sendRSTP == true))
	{
		// See in 802.1Q-2018:
		//  - 12.8.1.1.3, b) and c);
		//  - 12.8.1.2.3, c) and d).
		if (bridge->callbacks.onTopologyChange)
		{
			bool allZero = true;
			for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
				allZero &= (bridge->ports[portIndex]->trees[givenTree]->tcWhile == 0);
			if (allZero)
				bridge->callbacks.onTopologyChange (bridge, (unsigned int) givenTree, timestamp);
		}

		portTree->tcWhile = 1 + port->trees [CIST_INDEX]->portTimes.HelloTime;

		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}

	if ((portTree->tcWhile == 0) && (port->sendRSTP == false))
	{
		portTree->tcWhile = bridge->trees [givenTree]->rootTimes.MaxAge + bridge->trees [givenTree]->rootTimes.ForwardDelay;
	}
}

// ============================================================================
// 13.27.k) - 13.27.11
// Using local parameters, this procedure simulates the processing that would be applied by rcvInfo() and
// rcvMsgs() to a BPDU received on the port, from the same region and with the following parameters:
//
// a) Message Age, Max Age, Hello Time and Forward Delay are derived from BridgeTimes (13.24.3);
// b) The CIST information carries the message priority vector (13.9) with a value of {pseudoRootId, 0,
//    pseudoRootId, 0, 0, 0};
// c) A CIST Port Role of Designated Port, with the Learning and Forwarding flags set;
// d) The Version 1 Length is 0 and Version 3 Length calculated appropriately;
// e) For each MSTI configured on the bridge, the corresponding MSTI Configuration Message carries:
//    1) A message priority vector with a value of {pseudoRootId, 0, 0, 0};
//    2) A Port Role of Designated Port, with the Learning and Forwarding flags set;
//    3) MSTI Remaining Hops set to the value of the MaxHops component of BridgeTimes (13.24.3).
//
// NOTE-If two L2GP ports are configured with the same CIST pseudoRootId then the IST may partition within the MST
// Region, but either of the L2GP ports can be selected to provide conectivity from the Region/customer network to a
// provider's network on an MSTI by MSTI basis.
void pseudoRcvMsgs (STP_BRIDGE* bridge, PortIndex givenPort)
{
	assert (false); // not implemented
}

// ============================================================================
// 13.27.l) - 13.27.12
RCVD_INFO rcvInfo (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	// Returns SuperiorDesignatedInfo if, for a given port and tree (CIST, or MSTI):
	// a) The received CIST or MSTI message conveys a Designated Port Role, and
	//    1) The message priority (msgPriority-13.25.26) is superior (13.9 or 13.10) to the port's port
	//       priority vector, or
	//    2) The message priority is the same as the port's port priority vector, and any of the received timer
	//       parameter values (msgTimes-13.25.27) differ from those already held for the port
	//       (portTimes-13.25.34).
	if (portTree->msgFlagsPortRole == BPDU_PORT_ROLE_DESIGNATED)
	{
		if (   portTree->msgPriority.IsSuperiorTo (portTree->portPriority)
			|| ((portTree->msgPriority == portTree->portPriority) && (portTree->msgTimes != portTree->portTimes)))
		{
//LOG (bridge, givenPort, givenTree, "-------------------------\r\n");
//LOG (bridge, givenPort, givenTree, "{S}: portTree->msgPriority.IsSuperiorTo (portTree->portPriority)\r\n", port->debugName);
//LOG (bridge, givenPort, givenTree, "{S}: portTree->msgPriority  = {PVS}\r\n", port->debugName, &portTree->msgPriority);
//LOG (bridge, givenPort, givenTree, "{S}: portTree->portPriority = {PVS}\r\n", port->debugName, &portTree->portPriority);
//LOG (bridge, givenPort, givenTree, "{S}: portTree->msgTimes  = {TMS}\r\n", port->debugName, &portTree->msgTimes);
//LOG (bridge, givenPort, givenTree, "{S}: portTree->portTimes = {TMS}\r\n", port->debugName, &portTree->portTimes);
//LOG (bridge, givenPort, givenTree, "-------------------------\r\n");
			return RCVD_INFO_SUPERIOR_DESIGNATED;
		}
	}

	// Otherwise, returns RepeatedDesignatedInfo if, for a given port and tree (CIST, or MSTI):
	// b) The received CIST or MSTI message conveys a Designated Port Role, and
	//    1) A message priority vector and timer parameters that are the same as the port's port priority
	//       vector and timer values; and
	//    2) infoIs is Received.
	if (   (portTree->msgFlagsPortRole == BPDU_PORT_ROLE_DESIGNATED)
		&& ((portTree->msgPriority == portTree->portPriority) && (portTree->msgTimes == portTree->portTimes))
		&& (portTree->infoIs == INFO_IS_RECEIVED))
	{
		return RCVD_INFO_REPEATED_DESIGNATED;
	}

	// Otherwise, returns InferiorDesignatedInfo if, for a given port and tree (CIST, or MSTI):
	// c) The received CIST or MSTI message conveys a Designated Port Role.
	if (portTree->msgFlagsPortRole == BPDU_PORT_ROLE_DESIGNATED)
	{
		return RCVD_INFO_INFERIOR_DESIGNATED;
	}

	// Otherwise, returns InferiorRootAlternateInfo if, for a given port and tree (CIST, or MSTI):
	// d) The received CIST or MSTI message conveys a Root Port, Alternate Port, or Backup Port Role and
	//    a CIST or MSTI message priority that is the same as or worse than the CIST or MSTI port priority
	//    vector.
	if (   ((portTree->msgFlagsPortRole == BPDU_PORT_ROLE_ROOT) || (portTree->msgFlagsPortRole == BPDU_PORT_ROLE_ALT_BACKUP))
		&& (portTree->msgPriority.IsWorseThanOrSameAs (portTree->portPriority)))
	{
		return RCVD_INFO_INFERIOR_ROOT_ALTERNATE;
	}

	// Otherwise, returns OtherInfo.
	return RCVD_INFO_OTHER;
}

// ============================================================================
// 13.27.m) - 13.27.13 in 802.1Q-2011
void rcvMsgs (STP_BRIDGE* bridge, PortIndex givenPort)
{
	PORT* port = bridge->ports [givenPort];

	// This procedure is invoked by the Port Receive state machine (13.29) to decode a received BPDU. Sets
	// rcvdTcn and rcvdTc for each and every MSTI if a TCN BPDU has been received, and extracts the message
	// priority and timer values from the received BPDU storing them in the msgPriority and msgTimes variables.
	if (bridge->receivedBpduType == VALIDATED_BPDU_TYPE_STP_TCN)
	{
		port->rcvdTcn = true;

		for (unsigned int treeIndex = 1; treeIndex < bridge->treeCount(); treeIndex++)
			port->trees [treeIndex]->rcvdTc = true;
	}
	else if ((bridge->receivedBpduType == VALIDATED_BPDU_TYPE_STP_CONFIG)
		||   (bridge->receivedBpduType == VALIDATED_BPDU_TYPE_RST)
		||   (bridge->receivedBpduType == VALIDATED_BPDU_TYPE_MST))
	{
		PORT_TREE* portCistTree = port->trees [CIST_INDEX];

		// See 13.25.26 in 802.1Q-2011

		// priority
		// See the definition of "port priority vector" in "13.9 CIST Priority Vector calculations" in 802.1Q-2011
		portCistTree->msgPriority.RootId				= bridge->receivedBpduContent->cistRootId;
		portCistTree->msgPriority.ExternalRootPathCost	= bridge->receivedBpduContent->cistExternalPathCost;
		portCistTree->msgPriority.RegionalRootId		= bridge->receivedBpduContent->cistRegionalRootId;
		if (port->rcvdInternal)
		{
			portCistTree->msgPriority.InternalRootPathCost = bridge->receivedBpduContent->cistInternalRootPathCost;
			portCistTree->msgPriority.DesignatedBridgeId   = bridge->receivedBpduContent->cistBridgeId;
		}
		else
		{
			portCistTree->msgPriority.InternalRootPathCost = 0;
			portCistTree->msgPriority.DesignatedBridgeId = bridge->receivedBpduContent->cistRegionalRootId;
		}
		portCistTree->msgPriority.DesignatedPortId		= bridge->receivedBpduContent->cistPortId;

		// times
		portCistTree->msgTimes.ForwardDelay = bridge->receivedBpduContent->ForwardDelay.GetValue () / 256;
		portCistTree->msgTimes.HelloTime    = bridge->receivedBpduContent->HelloTime.GetValue () / 256;
		portCistTree->msgTimes.MaxAge       = bridge->receivedBpduContent->MaxAge.GetValue () / 256;
		portCistTree->msgTimes.MessageAge   = bridge->receivedBpduContent->MessageAge.GetValue () / 256;
		// This "if" condition deviates from the standard. The standard says "If the BPDU is an STP or RST BPDU
		// without MSTP parameters", but I'm pretty sure also BPDUs from a different MST region should be treated
		// the same. A false value in rcvdInternal covers both cases.
		if (port->rcvdInternal)
			portCistTree->msgTimes.remainingHops = bridge->receivedBpduContent->cistRemainingHops;
		else
			portCistTree->msgTimes.remainingHops = bridge->trees[CIST_INDEX]->BridgeTimes.remainingHops;

		// flags
		if (bridge->receivedBpduType == VALIDATED_BPDU_TYPE_STP_CONFIG)
		{
			portCistTree->msgFlagsTc            = GetBpduFlagTc    (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsTcAckOrMaster = GetBpduFlagTcAck (bridge->receivedBpduContent->cistFlags);

			// From the note at the end of 13.27.12 in 802.1Q-2011:
			// A Configuration BPDU implicitly conveys a Designated Port Role.
			portCistTree->msgFlagsPortRole      = BPDU_PORT_ROLE_DESIGNATED;

			// flags below are not present in a Config BPDU, but let's clear them nevertheless.
			portCistTree->msgFlagsProposal      = false;
			portCistTree->msgFlagsLearning      = false;
			portCistTree->msgFlagsForwarding    = false;
			portCistTree->msgFlagsAgreement     = false;
		}
		else
		{
			portCistTree->msgFlagsTc            = GetBpduFlagTc         (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsProposal      = GetBpduFlagProposal   (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsPortRole      = GetBpduFlagPortRole   (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsLearning      = GetBpduFlagLearning   (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsForwarding    = GetBpduFlagForwarding (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsAgreement     = GetBpduFlagAgreement  (bridge->receivedBpduContent->cistFlags);
			portCistTree->msgFlagsTcAckOrMaster = false;
		}
	}
	else
		assert (false);

	// The procedure sets rcvdMsg for the CIST, and makes the received CST or CIST message available to the
	// CIST Port Information state machines.
	bridge->ports [givenPort]->trees [CIST_INDEX]->rcvdMsg = true;

	// If and only if rcvdInternal is set, this procedure sets rcvdMsg for each and every MSTI for which a MSTI
	// message is conveyed in the BPDU, and makes available each MSTI message and the common parts of the
	// CIST message priority (the CIST Root Identifier, External Root Path Cost, and Regional Root Identifier) to
	// the Port Information state machine for that MSTI.
	if (port->rcvdInternal)
	{
		LOG (bridge, -1, -1, "rcvMsgs() -- rcvdInternal==1\r\n");

		// these assert conditions should have been checked while validating the received bpdu
		unsigned short version3Length = bridge->receivedBpduContent->Version3Length.GetValue ();
		unsigned short version3Offset = (unsigned short) offsetof (struct MSTP_BPDU, mstConfigId);
		unsigned short version3CistLength = (unsigned short) sizeof (MSTP_BPDU) - version3Offset;
		unsigned short mstiLength = version3Length - version3CistLength;
		assert ((mstiLength % sizeof (MSTI_CONFIG_MESSAGE)) == 0);

		unsigned int mstiMessageCount = mstiLength / sizeof (MSTI_CONFIG_MESSAGE);

		const MSTI_CONFIG_MESSAGE* mstiMessages = (MSTI_CONFIG_MESSAGE*) (bridge->receivedBpduContent + 1);

		for (unsigned int messageIndex = 0; messageIndex < mstiMessageCount; messageIndex++)
		{
			const MSTI_CONFIG_MESSAGE* message = &mstiMessages [messageIndex];

			unsigned int mstid = 1 + messageIndex;

			PORT_TREE* portTree = port->trees [mstid];

			// first two components are always zero for MSTIs
			// portTree->msgPriority.RootId
			// portTree->msgPriority.ExternalRootPathCost
			portTree->msgPriority.RegionalRootId		= message->RegionalRootId;
			portTree->msgPriority.InternalRootPathCost	= message->InternalRootPathCost;

			// TODO: not sure about the lines below
			portTree->msgPriority.DesignatedBridgeId.SetPriority (message->BridgePriority << 8, mstid);
			portTree->msgPriority.DesignatedBridgeId.SetAddress (bridge->receivedBpduContent->cistBridgeId.GetAddress().bytes);
			portTree->msgPriority.DesignatedPortId.Set (message->PortPriority & 0xF0, bridge->receivedBpduContent->cistPortId.GetPortNumber ());

			portTree->msgTimes.remainingHops = message->RemainingHops;

			portTree->msgFlagsTc            = GetBpduFlagTc         (message->flags);
			portTree->msgFlagsProposal      = GetBpduFlagProposal   (message->flags);
			portTree->msgFlagsPortRole      = GetBpduFlagPortRole   (message->flags);
			portTree->msgFlagsLearning      = GetBpduFlagLearning   (message->flags);
			portTree->msgFlagsForwarding    = GetBpduFlagForwarding (message->flags);
			portTree->msgFlagsAgreement     = GetBpduFlagAgreement  (message->flags);
			portTree->msgFlagsTcAckOrMaster = GetBpduFlagMaster     (message->flags);

			portTree->rcvdMsg = true;
		}
	}
	else
	{
		LOG (bridge, -1, -1, "rcvMsgs() -- rcvdInternal==0\r\n");
	}
}

// ============================================================================
// 13.27.n) - 13.27.14

void recordAgreement (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgFlags below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* cistPortTree = port->trees [CIST_INDEX];
	PORT_TREE* portTree = port->trees [givenTree];

	if (givenTree == CIST_INDEX)
	{
		// For the CIST and a given port, if rstpVersion is TRUE, operPointToPointMAC (6.6.3) is TRUE, and the
		// received CIST Message has the Agreement flag set, then the CIST agreed flag is set and the CIST proposing
		// flag is cleared. Otherwise the CIST agreed flag is cleared. Additionally, if the CIST message was received
		// from a bridge in a different MST Region, i.e., the rcvdInternal flag is clear, the agreed and proposing flags
		// for this port for all MSTIs are set or cleared to the same value as the CIST agreed and proposing flags. If the
		// CIST message was received from a bridge in the same MST Region, the MSTI agreed and proposing flags
		// are not changed.

		if (rstpVersion (bridge) && port->operPointToPointMAC && portTree->msgFlagsAgreement)
		{
			portTree->agreed = true;
			portTree->proposing = false;
		}
		else
		{
			portTree->agreed = false;
		}

		if (port->rcvdInternal == false)
		{
			for (unsigned int treeIndex = 1; treeIndex < bridge->treeCount(); treeIndex++)
			{
				port->trees [treeIndex]->agreed    = cistPortTree->agreed;
				port->trees [treeIndex]->proposing = cistPortTree->proposing;
			}
		}
	}
	else
	{
		// For a given MSTI and port, if operPointToPointMAC (6.6.3) is TRUE, and
		//
		// a) The message priority vector of the CIST Message accompanying the received MSTI Message (i.e.,
		//    received in the same BPDU) has the same CIST Root Identifier, CIST External Root Path Cost, and
		//    Regional Root Identifier as the CIST port priority vector, and
		// b) The received MSTI Message has the Agreement flag set,
		//
		// the MSTI agreed flag is set and the MSTI proposing flag is cleared. Otherwise the MSTI agreed flag is
		// cleared.
		//
		// NOTE-MSTI Messages received from bridges external to the MST Region are discarded and not processed by
		// recordAgreeement() or recordProposal().

		// Note AG about the NOTE above: Passive constructs are bad bad bad. By whom are they discarded? Are we supposed to discard them here?
		assert (port->rcvdInternal); // Let's assume they're discarded outside of this function, and that this function is not called.

		if (   port->operPointToPointMAC
			&& (cistPortTree->msgPriority.RootId               == cistPortTree->portPriority.RootId)
			&& (cistPortTree->msgPriority.ExternalRootPathCost == cistPortTree->portPriority.ExternalRootPathCost)
			&& (cistPortTree->msgPriority.RegionalRootId       == cistPortTree->portPriority.RegionalRootId)
			&& portTree->msgFlagsAgreement)
		{
			portTree->agreed = true;
			portTree->proposing = false;
		}
		else
			portTree->agreed = false;
	}
}

// ============================================================================
// 13.27.o) - 13.27.15
// For the CIST and a given port, if the CIST message has the learning flag set:
// a) The disputed variable is set; and
// b) The agreed variable is cleared.
//
// Additionally, if the CIST message was received from a bridge in a different MST region (i.e., if the
// rcvdInternal flag is clear), then for all the MSTIs:
// c) The disputed variable is set; and
// d) The agreed variable is cleared.
//
// For a given MSTI and port, if the received MSTI message has the learning flag set:
// e) The disputed variable is set; and
// f) The agreed variable is cleared.
void recordDispute (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgFlags below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	// Not clear: The condition for c/d is a sub-condition of the condition for a/b? Or the two conditions are independent?
	// We'll consider it a sub-condition in the code below,
	// cause the text for independent conditions could / would have been expressed in a much simpler way.

	if (givenTree == CIST_INDEX)
	{
		if (portTree->msgFlagsLearning)
		{
			portTree->disputed = true;
			portTree->agreed = false;

			if (port->rcvdInternal == 0)
			{
				for (unsigned int treeIndex = 1; treeIndex < bridge->treeCount(); treeIndex++)
				{
					port->trees [treeIndex]->disputed = true;
					port->trees [treeIndex]->agreed = false;
				}
			}
		}
	}
	else
	{
		if (portTree->msgFlagsLearning)
		{
			portTree->disputed = true;
			portTree->agreed = false;
		}
	}
}

// ============================================================================
// 13.27.p) - 13.27.16
// For the CIST and a given port, if the CIST message was received from a bridge in a different MST Region,
// i.e. the rcvdInternal flag is clear, the mastered variable for this port is cleared for all MSTIs.
//
// For a given MSTI and port, if the MSTI message was received on a point-to-point link and the MSTI
// Message has the Master flag set, set the mastered variable for this MSTI. Otherwise reset the mastered
// variable.
void recordMastered (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgFlags below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];

	if (givenTree == CIST_INDEX)
	{
		if (port->rcvdInternal == false)
		{
			for (unsigned int treeIndex = 1; treeIndex < bridge->treeCount(); treeIndex++)
				port->mastered = false;
		}
	}
	else
	{
		if (port->operPointToPointMAC && port->trees [givenTree]->msgFlagsTcAckOrMaster)
			port->mastered = true;
		else
			port->mastered = false;
	}

	LOG (bridge, givenPort, givenTree, "Port {D}: {TN}: recordMastered(): {D}\r\n", 1 + givenPort, givenTree, (int) port->mastered);
}

// ============================================================================
// 13.27.q) - 13.27.17
// Sets the components of the portPriority variable to the values of the corresponding msgPriority components.
void recordPriority (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgPriority below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	portTree->portPriority = portTree->msgPriority;

	LOG (bridge, givenPort, givenTree, "Port {D}: {TN}: recordPriority(): {PVS}\r\n", 1 + givenPort, givenTree, &portTree->portPriority);
}

// ============================================================================
// 13.27.r) - 13.27.18
// For the CIST and a given port, if the received CIST Message conveys a Designated Port Role, and has the
// Proposal flag set, the CIST proposed flag is set. Otherwise the CIST proposed flag is not changed.
// Additionally, if the CIST Message was received from a bridge in a different MST Region, i.e., the
// rcvdInternal flag is clear, the proposed flags for this port for all MSTIs are set or cleared to the same value as
// the CIST proposed flag. If the CIST message was received from a bridge in the same MST Region, the
// MSTI proposed flags are not changed.
//
// For a given MSTI and port, if the received MSTI Message conveys a Designated Port Role, and has the
// Proposal flag set, the MSTI proposed flag is set. Otherwise the MSTI proposed flag is not changed.
void recordProposal (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgFlags below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	if (givenTree == CIST_INDEX)
	{
		if ((portTree->msgFlagsPortRole == BPDU_PORT_ROLE_DESIGNATED) && portTree->msgFlagsProposal)
		{
			portTree->proposed = 1;

			if (port->rcvdInternal == 0)
			{
				for (unsigned int mstiIndex = 1; mstiIndex < bridge->treeCount(); mstiIndex++)
					port->trees [mstiIndex]->proposed = port->trees [CIST_INDEX]->proposed;
			}
		}
	}
	else
	{
		if ((portTree->msgFlagsPortRole == BPDU_PORT_ROLE_DESIGNATED) && portTree->msgFlagsProposal)
			portTree->proposed = 1;
	}
}

// ============================================================================
// 13.27.s) - 13.27.19
// For the CIST and a given port, sets portTimes' Message Age, Max Age, Forward Delay, and remainingHops
// to the received values held in msgTimes and portTimes' Hello Time to the default specified in Table 13-5.
//
// For a given MSTI and port, sets portTime's remainingHops to the received value held in msgTimes.
void recordTimes (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgTimes below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	if (givenTree == CIST_INDEX)
	{
		portTree->portTimes.MessageAge    = portTree->msgTimes.MessageAge;
		portTree->portTimes.MaxAge        = portTree->msgTimes.MaxAge;
		portTree->portTimes.ForwardDelay  = portTree->msgTimes.ForwardDelay;
		portTree->portTimes.remainingHops = portTree->msgTimes.remainingHops;
		portTree->portTimes.HelloTime     = 2;
	}
	else
	{
		portTree->portTimes.remainingHops = portTree->msgTimes.remainingHops;
	}
}

// ============================================================================
// 13.27.t) - 13.27.20
// Sets reRoot TRUE for this tree (the CIST or a given MSTI) for all ports of the bridge.
void setReRootTree (STP_BRIDGE* bridge, TreeIndex givenTree)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		bridge->ports [portIndex]->trees [givenTree]->reRoot = true;
}

// ============================================================================
// 13.27.u) - 13.27.21
// Sets selected TRUE for this tree (the CIST or a given MSTI) for all ports of the bridge if reselect is FALSE
// for all ports in this tree. If reselect is TRUE for any port in this tree, this procedure takes no action.
void setSelectedTree (STP_BRIDGE* bridge, TreeIndex givenTree)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		if (bridge->ports [portIndex]->trees [givenTree]->reselect)
			return;
	}

	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		bridge->ports [portIndex]->trees [givenTree]->selected = true;
}

// ============================================================================
// 13.27.v) - 13.27.22
// Sets sync TRUE for this tree (the CIST or a given MSTI) for all ports of the bridge.
void setSyncTree (STP_BRIDGE* bridge, TreeIndex givenTree)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		bridge->ports [portIndex]->trees [givenTree]->sync = true;
}

// ============================================================================
// 13.27.w) - 13.27.23
// For the CIST and a given port:
// a) If the Topology Change Acknowledgment flag is set for the CIST in the received BPDU, sets
//    rcvdTcAck TRUE.
// b) If rcvdInternal is clear and the Topology Change flag is set for the CIST in the received BPDU, sets
//    rcvdTc TRUE for the CIST and for each and every MSTI.
// c) If rcvdInternal is set, sets rcvdTc for the CIST if the Topology Change flag is set for the CIST in the
//    received BPDU.
//
// For a given MSTI and port, sets rcvdTc for this MSTI if the Topology Change flag is set in the corresponding
// MSTI message.
void setTcFlags (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// we're accessing msgFlags below, which is valid only when a received BPDU is being handled
	assert (bridge->receivedBpduContent != nullptr);

	PORT* port = bridge->ports [givenPort];

	if (givenTree == CIST_INDEX)
	{
		PORT_TREE* cistTree = port->trees [CIST_INDEX];

		if (cistTree->msgFlagsTcAckOrMaster)
			port->rcvdTcAck = true;

		if ((port->rcvdInternal == false) && cistTree->msgFlagsTc)
		{
			for (unsigned int treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
				port->trees [treeIndex]->rcvdTc = true;
		}

		if (port->rcvdInternal)
		{
			if (cistTree->msgFlagsTc)
				cistTree->rcvdTc = true;
		}
	}
	else
	{
		PORT_TREE* portTree = port->trees [givenTree];

		if (portTree->msgFlagsTc)
			portTree->rcvdTc = true;
	}
}

// ============================================================================
// 13.27.x) - 13.27.24
// If and only if restrictedTcn is FALSE for the port that invoked the procedure, sets tcProp TRUE for the given
// tree (the CIST or a given MSTI) for all other ports.
void setTcPropTree (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	if (bridge->ports [givenPort]->restrictedTcn == false)
	{
		for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		{
			if (portIndex != (unsigned int) givenPort)
				bridge->ports [portIndex]->trees [givenTree]->tcProp = true;
		}
	}
}

// ============================================================================
// 13.27.y) - 13.27.25
// For all MSTIs, for each port that has infoInternal set:
// a) Clears the agree, agreed, and synced variables; and
// b) Sets the sync variable.
void syncMaster (STP_BRIDGE* bridge)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		PORT* port = bridge->ports [portIndex];

		if (port->infoInternal)
		{
			for (unsigned int treeIndex = 1; treeIndex < bridge->treeCount(); treeIndex++)
			{
				PORT_TREE* portTree = port->trees [treeIndex];
				portTree->agree = false;
				portTree->agreed = false;
				portTree->synced = false;
				portTree->sync = true;
			}
		}
	}
}

// ============================================================================
// 13.27.z) - 13.27.26
// Transmits a Configuration BPDU. The first four components of the message priority vector (13.25.26)
// conveyed in the BPDU are set to the value of the CIST Root Identifier, External Root Path Cost, Bridge
// Identifier, and Port Identifier components of the CIST's designatedPriority parameter (13.25.7) for this port.
// The topology change flag is set if (tcWhile != 0) for the port. The topology change acknowledgment flag is
// set to the value of tcAck for the port. The remaining flags are set to zero. The value of the Message Age, Max
// Age, and Fwd Delay parameters conveyed in the BPDU are set to the values held in the CIST's
// designatedTimes parameter (13.25.8) for the port. The value of the Hello Time parameter conveyed in the
// BPDU is set to the value held in the CIST's portTimes parameter (13.25.34) for the port.
void txConfig (STP_BRIDGE* bridge, PortIndex givenPort, unsigned int timestamp)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* cistTree = port->trees [CIST_INDEX];

	unsigned int bpduSize = (unsigned int) offsetof (MSTP_BPDU, Version1Length);

	FLUSH_LOG (bridge);

	MSTP_BPDU* bpdu = (MSTP_BPDU*) bridge->callbacks.transmitGetBuffer (bridge, givenPort, bpduSize, timestamp);
	if (bpdu != NULL)
	{
		// 9.3.1 in 802.1D-2004 (not 2011!)
		bpdu->protocolId = 0;
		bpdu->protocolVersionId = 0;
		bpdu->bpduType = 0;

		bpdu->cistRootId           = cistTree->designatedPriority.RootId;
		bpdu->cistExternalPathCost = cistTree->designatedPriority.ExternalRootPathCost;
		bpdu->cistRegionalRootId   = cistTree->designatedPriority.DesignatedBridgeId;
		bpdu->cistPortId           = cistTree->designatedPriority.DesignatedPortId;

		bpdu->cistFlags = 0;

		if (cistTree->tcWhile != 0)
			bpdu->cistFlags |= (unsigned char) 1;

		if (port->tcAck)
			bpdu->cistFlags |= (unsigned char) 0x80;

		bpdu->MessageAge   = cistTree->designatedTimes.MessageAge * 256;
		bpdu->MaxAge       = cistTree->designatedTimes.MaxAge * 256;
		bpdu->ForwardDelay = cistTree->designatedTimes.ForwardDelay * 256;
		bpdu->HelloTime    = cistTree->portTimes.HelloTime * 256;

		#if STP_USE_LOG
			LOG (bridge, givenPort, -1, "TX Config BPDU to port {D}:\r\n", 1 + givenPort);
			LOG_INDENT (bridge);
			DumpConfigBpdu (bridge, givenPort, -1, bpdu);
			LOG_UNINDENT (bridge);

			FLUSH_LOG (bridge);
		#endif
		bridge->callbacks.transmitReleaseBuffer (bridge, bpdu);
	}
}

// ============================================================================
// 13.27.aa) - 13.27.27
void txRstp (STP_BRIDGE* bridge, PortIndex givenPort, unsigned int timestamp)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* cistTree = port->trees [CIST_INDEX];

	unsigned int bpduSize;
	if (bridge->ForceProtocolVersion < 3)
		bpduSize = (unsigned int) offsetof (struct MSTP_BPDU, Version3Length);
	else
		bpduSize = sizeof(MSTP_BPDU) + bridge->mstiCount * sizeof(MSTI_CONFIG_MESSAGE);

	FLUSH_LOG (bridge);

	MSTP_BPDU* bpdu = (MSTP_BPDU*) bridge->callbacks.transmitGetBuffer (bridge, givenPort, bpduSize, timestamp);
	if (bpdu != nullptr)
	{
		// octets 1 and 2 - 14.5 in 802.1Q
		bpdu->protocolId = 0;

		// octets 3 and 4
		if (bridge->ForceProtocolVersion < 3)
		{
			// 14.5.c)
			bpdu->protocolVersionId = 2;
			bpdu->bpduType = 2;
		}
		else
		{
			// 14.5.d)
			bpdu->protocolVersionId = 3;
			bpdu->bpduType = 2;
		}

		// octet 5 - 14.6.a) to 14.6.h)
		bpdu->cistFlags = GetBpduPortRole (cistTree->role) << 2;
		if (cistTree->agree)
			bpdu->cistFlags |= (unsigned char) 0x40;

		if (cistTree->proposing)
			bpdu->cistFlags |= (unsigned char) 2;

		if (cistTree->tcWhile != 0)
			bpdu->cistFlags |= (unsigned char) 1;

		if (cistTree->learning)
			bpdu->cistFlags |= (unsigned char) 0x10;

		if (cistTree->forwarding)
			bpdu->cistFlags |= (unsigned char) 0x20;

		// octets 6 to 13 - 14.6.h)
		bpdu->cistRootId = cistTree->designatedPriority.RootId;

		// octets 14 to 17 - 14.6.i)
		bpdu->cistExternalPathCost = cistTree->designatedPriority.ExternalRootPathCost;

		// octets 18 to 25 - 14.6.j)
		bpdu->cistRegionalRootId = cistTree->designatedPriority.RegionalRootId;

		// octets 26 to 27 - 14.6.k)
		bpdu->cistPortId = cistTree->designatedPriority.DesignatedPortId;

		// octets 28 to 29 - 14.6.l)
		bpdu->MessageAge = cistTree->designatedTimes.MessageAge * 256;

		// octets 30 to 31 - 14.6.m)
		bpdu->MaxAge = cistTree->designatedTimes.MaxAge * 256;

		// octets 32 to 33 - 14.6.n)
		bpdu->HelloTime = cistTree->portTimes.HelloTime * 256;

		// octets 34 to 35 - 14.6.o)
		bpdu->ForwardDelay = cistTree->designatedTimes.ForwardDelay * 256;

		// octet 36 - 14.6.p)
		bpdu->Version1Length = 0;

		if (bridge->ForceProtocolVersion >= 3)
		{
			// octet 37 to 38 - 14.6.q)
			bpdu->Version3Length = (unsigned short) (bpduSize - 38);

			// octet 39 to 89 - 14.6.r)
			bpdu->mstConfigId = bridge->MstConfigId;

			// octet 90 to 93 - 14.6.s)
			bpdu->cistInternalRootPathCost	= cistTree->designatedPriority.InternalRootPathCost;

			// octet 94 to 101 - 14.6.t)
			bpdu->cistBridgeId				= cistTree->designatedPriority.DesignatedBridgeId;

			// octet 102 - 14.6.u)
			bpdu->cistRemainingHops			= cistTree->designatedTimes.remainingHops;

			MSTI_CONFIG_MESSAGE* mstiMessage = (MSTI_CONFIG_MESSAGE*) (bpdu + 1);

			for (unsigned int mstiIndex = 0; mstiIndex < bridge->mstiCount; mstiIndex++)
			{
				const PORT_TREE* tree = port->trees [1 + mstiIndex];

				mstiMessage->flags = GetBpduPortRole (tree->role) << 2;

				if (tree->agree)
					mstiMessage->flags |= (unsigned char) 0x40;

				if (tree->proposing)
					mstiMessage->flags |= (unsigned char) 2;

				if (tree->tcWhile != 0)
					mstiMessage->flags |= (unsigned char) 1;

				// 13.25.23
				if (port->master)
					mstiMessage->flags |= (unsigned char) 0x80;

				if (tree->learning)
					mstiMessage->flags |= (unsigned char) 0x10;

				if (tree->forwarding)
					mstiMessage->flags |= (unsigned char) 0x20;

				mstiMessage->RegionalRootId			= tree->designatedPriority.RegionalRootId;
				mstiMessage->InternalRootPathCost	= tree->designatedPriority.InternalRootPathCost;
				mstiMessage->BridgePriority			= (bridge->trees [1 + mstiIndex]->GetBridgeIdentifier().GetPriority() & 0xF000) >> 8;
				mstiMessage->PortPriority			= tree->portId.GetPriority ();

				mstiMessage->RemainingHops		= tree->designatedTimes.remainingHops;

				mstiMessage++;
			}
		}

		#if STP_USE_LOG
			if (bridge->ForceProtocolVersion < 3)
			{
				LOG (bridge, givenPort, -1, "TX RSTP BPDU to port {D}:\r\n", 1 + givenPort);
				LOG_INDENT (bridge);
				DumpRstpBpdu (bridge, givenPort, -1, bpdu);
				LOG_UNINDENT (bridge);
			}
			else
			{
				LOG (bridge, givenPort, -1, "TX MSTP BPDU to port {D}:\r\n", 1 + givenPort);
				LOG_INDENT (bridge);
				DumpMstpBpdu (bridge, givenPort, -1, bpdu);
				LOG_UNINDENT (bridge);
			}

			FLUSH_LOG (bridge);
		#endif

		bridge->callbacks.transmitReleaseBuffer (bridge, bpdu);
	}
}

// ============================================================================
// 13.27.ab) - 13.27.28
void txTcn (STP_BRIDGE* bridge, PortIndex givenPort, unsigned int timestamp)
{
	FLUSH_LOG (bridge);

	BPDU_HEADER* bpdu = (BPDU_HEADER*) bridge->callbacks.transmitGetBuffer (bridge, givenPort, sizeof (BPDU_HEADER), timestamp);
	if (bpdu != nullptr)
	{
		// 9.3.2 in 802.1D-2004 (not 2011!)
		bpdu->protocolId = 0;
		bpdu->protocolVersionId = 0;
		bpdu->bpduType = 0x80;

		LOG (bridge, givenPort, -1, "TX TCN BPDU to port {D}:\r\n", 1 + givenPort);
		FLUSH_LOG (bridge);

		bridge->callbacks.transmitReleaseBuffer (bridge, bpdu);
	}
}

// ============================================================================
// 13.27.ac) - 13.27.29
// Sets rcvdSTP TRUE if the BPDU received is a version 0 or version 1 TCN or a Config BPDU. Sets
// rcvdRSTP TRUE if the received BPDU is a RST BPDU or a MST BPDU.
void updtBPDUVersion (STP_BRIDGE* bridge, PortIndex givenPort)
{
	switch (bridge->receivedBpduType)
	{
		case VALIDATED_BPDU_TYPE_STP_TCN:
		case VALIDATED_BPDU_TYPE_STP_CONFIG:
			bridge->ports [givenPort]->rcvdSTP = true;
			break;

		case VALIDATED_BPDU_TYPE_MST:
		case VALIDATED_BPDU_TYPE_RST:
			bridge->ports [givenPort]->rcvdRSTP = true;
			break;

		default:
			assert (false);
	}
}

// ============================================================================
// 13.27.ad) - 13.27.30
// Updates rcvdInfoWhile (13.23). The value assigned to rcvdInfoWhile is three times the Hello Time, if either:
//
// a) Message Age, incremented by 1 second and rounded to the nearest whole second, does not exceed
//    Max Age and the information was received from a bridge external to the MST Region (rcvdInternal
//    FALSE);
// or
//
// b) remainingHops, decremented by one, is greater than zero and the information was received from a
//    bridge internal to the MST Region (rcvdInternal TRUE);
//
// and is zero otherwise.
//
// The values of Message Age, Max Age, remainingHops, and Hello Time used in these calculations are taken
// from the CIST's portTimes parameter (13.25.34) and are not changed by this procedure.
void updtRcvdInfoWhile	(STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	const TIMES* cistTimes = &port->trees [CIST_INDEX]->portTimes;

	if (((cistTimes->MessageAge + 1 <= cistTimes->MaxAge) && (port->rcvdInternal == false))
		|| ((cistTimes->remainingHops > 1) && port->rcvdInternal))
	{
		portTree->rcvdInfoWhile = 3 * cistTimes->HelloTime;
	}
	else
		portTree->rcvdInfoWhile = 0;
}

// ============================================================================

static void CalculateRootPathPriorityForPort (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree, PRIORITY_VECTOR* rootPathPriorityOut)
{
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	*rootPathPriorityOut = portTree->portPriority;

	if (givenTree == CIST_INDEX)
	{
		// 13.9, page 337 in 802.1Q-2011
		// A root path priority vector for a Port can be calculated from a port priority vector that contains information
		// from a message priority vector, as follows:

		// Note AG: The standard references 13.27.8 (fromSameRegion), but that function tries to read the received BPDU
		// outside of STP_OnBpduReceived. I replaced fromSameRegion with rcvdInternal in the "if" below.
		if (port->rcvdInternal == false)
		{
			// If the port priority vector was received from a bridge in a different region (13.27.8), the External Port Path
			// Cost EPCPB is added to the External Root Path Cost component, and the Regional Root Identifier is set to
			// the value of the Bridge Identifier for the receiving bridge. The Internal Root Path Cost component will have
			// been set to zero on reception.
			//		root path priority vector = {RD : ERCD + EPCPB : B : 0 : D : PD : PB}
			rootPathPriorityOut->ExternalRootPathCost += port->ExternalPortPathCost;
			rootPathPriorityOut->RegionalRootId = bridge->trees [givenTree]->GetBridgeIdentifier ();
			assert (portTree->portPriority.InternalRootPathCost.GetValue () == 0);
		}
		else
		{
			// If the port priority vector was received from a bridge in the same region (13.27.8), the Internal Port Path
			// Cost IPCPB is added to the Internal Root Path Cost component.
			//		root path priority vector = {RD : ERCD : RRD : IRCD + IPCPB : D : PD : PB)
			rootPathPriorityOut->InternalRootPathCost += portTree->InternalPortPathCost;
		}
	}
	else
	{
		// MSTI - 13.10, page 338 in 802.1Q-2011
		// A root path priority vector for a given MSTI can be calculated for a port that has received a port priority
		// vector from a bridge in the same region by adding the Internal Port Path Cost IPCPB to the Internal Root
		// Path Cost component.
		//			root path priority vector = {RRD : IRCD + IPCPB : D : PD : PB)

		assert (port->rcvdInternal);
		rootPathPriorityOut->InternalRootPathCost += portTree->InternalPortPathCost;
	}
}

// 13.25.7
static void CalculateDesignatedPriorityForPort (STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	BRIDGE_TREE* bridgeTree = bridge->trees [givenTree];
	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	if (givenTree == CIST_INDEX)
	{
		// The designated priority vector for a port Q on bridge B is the root priority vector with B's Bridge Identifier
		// B substituted for the DesignatedBridgeID and Q's Port Identifier QB substituted for the DesignatedPortID
		// and RcvPortID components.
		portTree->designatedPriority = bridgeTree->rootPriority;
		portTree->designatedPriority.DesignatedBridgeId = bridgeTree->GetBridgeIdentifier ();
		portTree->designatedPriority.DesignatedPortId   = portTree->portId;

		// If Q is attached to a LAN that has one or more STP bridges attached (as
		// determined by the Port Protocol Migration state machine), B's Bridge Identifier B is also substituted for the
		// RRootID component.
		if (port->sendRSTP == false)
		{
			portTree->designatedPriority.RegionalRootId = bridgeTree->GetBridgeIdentifier ();
		}
	}
	else
	{
		// MSTI
		// The designated priority vector for a port Q on bridge B is the root priority vector with B's Bridge Identifier
		// B substituted for the DesignatedBridgeID and Q's Port Identifier QB substituted for the DesignatedPortID
		// and RcvPortID components.
		portTree->designatedPriority = bridgeTree->rootPriority;
		portTree->designatedPriority.DesignatedBridgeId	= bridgeTree->GetBridgeIdentifier ();
		portTree->designatedPriority.DesignatedPortId	= portTree->portId;
	}
}

// ============================================================================
// 13.27.ae) - 13.27.31
void updtRolesTree (STP_BRIDGE* bridge, TreeIndex givenTree)
{
	BRIDGE_TREE* bridgeTree = bridge->trees [givenTree];

	LOG (bridge, -1, givenTree, "Tree {D}:\r\n", givenTree);
	LOG (bridge, -1, givenTree, "  BridgeID: {BID}\r\n", &bridgeTree->GetBridgeIdentifier ());

	// these two variables are accessed only after "if ((givenTree == CIST_INDEX)"
	BRIDGE_ID previousCistRegionalRootIdentifier = bridgeTree->rootPriority.RegionalRootId;
	INV_UINT4 previousCistExternalRootPathCost   = bridgeTree->rootPriority.ExternalRootPathCost;

	// initialize this to our bridge priority
	bridgeTree->rootPriority = bridgeTree->GetBridgePriority ();
	bridgeTree->rootPortId.Reset ();
	bridgeTree->rootTimes = bridgeTree->BridgeTimes;

	PORT_TREE* rootPortTree = nullptr;

	for (PortIndex portIndex = (PortIndex)0; portIndex < bridge->portCount; portIndex++)
	{
		PORT* port = bridge->ports [portIndex];
		PORT_TREE* portTree = port->trees [givenTree];

		if (portTree->infoIs == INFO_IS_RECEIVED)
		{
			// a)
			PRIORITY_VECTOR rootPathPriority;
			CalculateRootPathPriorityForPort (bridge, portIndex, givenTree, &rootPathPriority);

			LOG (bridge, -1, givenTree, "  Port {D} root path priority  : {PVS}\r\n", 1 + portIndex, &rootPathPriority);

			// b)
			if ((rootPathPriority.DesignatedBridgeId.GetAddress () != bridgeTree->GetBridgePriority ().DesignatedBridgeId.GetAddress ())
				&& (port->restrictedRole == false))
			{
				if (rootPathPriority.IsBetterThan (bridgeTree->rootPriority)
					|| ((rootPathPriority == bridgeTree->rootPriority) && (portTree->portId.IsBetterThan (bridgeTree->rootPortId))))
				{
					rootPortTree = portTree;

					bridgeTree->rootPriority = rootPathPriority;
					bridgeTree->rootPortId   = portTree->portId;

					bridgeTree->rootTimes = portTree->portTimes;
					if (port->rcvdInternal == false)
						bridgeTree->rootTimes.MessageAge++;
					else
					{
						assert (bridgeTree->rootTimes.remainingHops > 0);
						bridgeTree->rootTimes.remainingHops--;
					}
				}
			}
		}
	}

	LOG (bridge, -1, givenTree, "  bridge root priority : {PVS}\r\n", &bridgeTree->rootPriority);
	LOG (bridge, -1, givenTree, "  root port = {PID}\r\n", &bridgeTree->rootPortId);

	// ------------------------------------------------------------------------

	for (PortIndex portIndex = (PortIndex)0; portIndex < bridge->portCount; portIndex++)
	{
		PORT* port = bridge->ports [portIndex];
		PORT_TREE* portTree = port->trees [givenTree];

		// d)
		CalculateDesignatedPriorityForPort (bridge, portIndex, givenTree);

		// e)
		portTree->designatedTimes = bridgeTree->rootTimes;

		LOG (bridge, -1, givenTree, "  Port {D} designated priority : {PVS}\r\n", 1 + portIndex, &portTree->designatedPriority);
	}

	// ------------------------------------------------------------------------

	// If the root priority vector for the CIST is recalculated, and has a different Regional Root Identifier than that
	// previously selected, and has or had a nonzero CIST External Root Path Cost, the syncMaster() procedure
	// (13.27.25) is invoked.
	if ((givenTree == CIST_INDEX)
		&& (previousCistRegionalRootIdentifier != bridgeTree->rootPriority.RegionalRootId)
		&& ((bridgeTree->rootPriority.ExternalRootPathCost.GetValue() != 0) || (previousCistExternalRootPathCost.GetValue() != 0)))
	{
		syncMaster (bridge);
	}

	// The CIST, or MSTI Port Role for each port is assigned, and its port priority vector and timer information are
	// updated as specified in the remainder of this clause (13.39.2).

	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		PORT* port = bridge->ports [portIndex];
		PORT_TREE* portTree = port->trees [givenTree];
		PORT_TREE* cistPortTree = port->trees [CIST_INDEX];

		// If the port is Disabled (infoIs == Disabled), selectedRole is set to DisabledPort.
		if (portTree->infoIs == INFO_IS_DISABLED)
		{
			portTree->selectedRole = STP_PORT_ROLE_DISABLED;
		}

		// Otherwise, if this procedure was invoked for an MSTI, for a port that is not Disabled, and that has CIST port
		// priority information that was received from a bridge external to its bridge's Region (infoIs == Received and
		// infoInternal == FALSE), then:
		else if (  (givenTree != CIST_INDEX)
				&& ((cistPortTree->infoIs == INFO_IS_RECEIVED) && (port->infoInternal == false)))
		{
			// f) If the selected CIST Port Role (calculated for the CIST prior to invoking this procedure for an
			//    MSTI) is RootPort, selectedRole is set to MasterPort.
			if (cistPortTree->selectedRole == STP_PORT_ROLE_ROOT)
				portTree->selectedRole = STP_PORT_ROLE_MASTER;

			// g) If selected CIST Port Role is AlternatePort, selectedRole is set to AlternatePort.
			if (cistPortTree->selectedRole == STP_PORT_ROLE_ALTERNATE)
				portTree->selectedRole = STP_PORT_ROLE_ALTERNATE;

			// h) Additionally, updtInfo is set if the port priority vector differs from the designated priority vector or
			//    the port's associated timer parameter differs from the one for the Root Port.
			//
			// Note AG: Problem in the standard: If we are the root bridge, we don't have a root port, so how are we
			// supposed to look at the "associated timer parameter" "for the Root Port"?
			// Let's look at the bridge times in this case.
			if (portTree->portPriority != portTree->designatedPriority)
			{
				portTree->updtInfo = true;
			}
			else if ((rootPortTree != nullptr) && (portTree->portTimes != rootPortTree->designatedTimes))
			{
				portTree->updtInfo = true;
			}
			else if ((rootPortTree == nullptr) && (portTree->portTimes != bridgeTree->rootTimes))
			{
				portTree->updtInfo = true;
			}
		}

		// Otherwise, for the CIST for a port that is not Disabled, or for an MSTI for a port that is not Disabled and
		// whose CIST port priority information was not received from a bridge external to the Region (infoIs !=
		// Received or infoInternal == TRUE), the CIST or MSTI port role is assigned, and the port priority vector and
		// timer information updated as follows:
		//
		// Note AG: this "else if" could be replaced with an "else", as the condition is the exact opposite of the condition in the "else if" above.
		else if (   (givenTree == CIST_INDEX)
				||  ((cistPortTree->infoIs != INFO_IS_RECEIVED) || (port->infoInternal == true)))
		{
			// Note AG: Let's use if / else if / else if... because each branch assigns a different port role, and it doesn't
			// make sense to assign it multiple times in the same run of this function. We'll end with "else assert (false);"

			// i) If the port priority vector information was aged (infoIs = Aged), updtInfo is set and selectedRole is
			//    set to DesignatedPort;
			if (portTree->infoIs == INFO_IS_AGED)
			{
				portTree->updtInfo = true;
				portTree->selectedRole = STP_PORT_ROLE_DESIGNATED;
			}

			// j) If the port priority vector was derived from another port on the bridge or from the bridge itself as the
			//    Root Bridge (infoIs = Mine), selectedRole is set to DesignatedPort. Additionally, updtInfo is set if the
			//    port priority vector differs from the designated priority vector or the port's associated timer
			//    parameter(s) differ(s) from the Root Port's associated timer parameters;
			//
			// See the note at condition h) above.
			else if (portTree->infoIs == INFO_IS_MINE)
			{
				portTree->selectedRole = STP_PORT_ROLE_DESIGNATED;

				if (portTree->portPriority != portTree->designatedPriority)
				{
					portTree->updtInfo = true;
				}
				else if ((rootPortTree != nullptr) && (portTree->portTimes != rootPortTree->designatedTimes))
				{
					portTree->updtInfo = true;
				}
				else if ((rootPortTree == nullptr) && (portTree->portTimes != bridgeTree->rootTimes))
				{
					portTree->updtInfo = true;
				}
			}

			// k) If the port priority vector was received in a Configuration Message and is not aged
			//    (infoIs == Received), and the root priority vector is now derived from it, selectedRole is set to
			//    RootPort, and updtInfo is reset;
			else if ((portTree->infoIs == INFO_IS_RECEIVED) && (rootPortTree == portTree))
			{
				portTree->selectedRole = STP_PORT_ROLE_ROOT;
				portTree->updtInfo = false;
			}

			// l) If the port priority vector was received in a Configuration Message and is not aged
			//    (infoIs == Received), the root priority vector is not now derived from it, the designated priority
			//    vector is not better than the port priority vector, and the designated bridge and designated port
			//    components of the port priority vector do not reflect another port on this bridge, selectedRole is set
			//    to AlternatePort, and updtInfo is reset;
			//
			// TODO: Question AG: What exactly is this supposed to mean?
			// "the designated bridge and designated port components of the port priority vector do not reflect another port on this bridge"
			// Answer: Let's look at the DesignatedBridgeId component, but only at the _address field, not at _priority too,
			// to account for the case when the bridge priority was just changed by the user (for instance from 0x8000 to 0x9000)
			// and a BPDU with the old priority is still propagating through the network.
			else if ((portTree->infoIs == INFO_IS_RECEIVED)
				&& (rootPortTree != portTree)
				&& (portTree->designatedPriority.IsNotBetterThan (portTree->portPriority))
				&& (portTree->portPriority.DesignatedBridgeId.GetAddress () != bridgeTree->GetBridgeIdentifier ().GetAddress ()))
			{
				portTree->selectedRole = STP_PORT_ROLE_ALTERNATE;
				portTree->updtInfo = false;
			}

			// m) If the port priority vector was received in a Configuration Message and is not aged
			//    (infoIs == Received), the root priority vector is not now derived from it, the designated priority
			//    vector is not better than the port priority vector, and the designated bridge and designated port
			//    components of the port priority vector reflect another port on this bridge, selectedRole is set to
			//    BackupPort, and updtInfo is reset;
			else if ((portTree->infoIs == INFO_IS_RECEIVED)
				&& (rootPortTree != portTree)
				&& (portTree->designatedPriority.IsNotBetterThan (portTree->portPriority))
				&& (portTree->portPriority.DesignatedBridgeId.GetAddress () == bridgeTree->GetBridgeIdentifier ().GetAddress ()))
			{
				portTree->selectedRole = STP_PORT_ROLE_BACKUP;
				portTree->updtInfo = false;
			}

			// n) If the port priority vector was received in a Configuration Message and is not aged
			//    (infoIs == Received), the root priority vector is not now derived from it, the designated priority
			//    vector is better than the port priority vector, selectedRole is set to DesignatedPort, and updtInfo is
			//    set.
			else if ((portTree->infoIs == INFO_IS_RECEIVED) && (rootPortTree != portTree)
				&& (portTree->designatedPriority.IsBetterThan (portTree->portPriority)))
			{
				portTree->selectedRole = STP_PORT_ROLE_DESIGNATED;
				portTree->updtInfo = true;
			}

			else
			{
				// should we ever be here??
				assert (false);
			}
		}

		LOG (bridge, -1, givenTree, "Port {D}: {TN}: selectedRole set to {S}\r\n", 1 + portIndex, givenTree, GetPortRoleName (portTree->selectedRole));
	}
}

// ============================================================================
// 13.27.af) - 13.27.32
// This procedure sets selectedRole to DisabledPort for all ports of the bridge for a given tree (CIST, or MSTI).
void updtRolesDisabledTree (STP_BRIDGE* bridge, TreeIndex givenTree)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		bridge->ports [portIndex]->trees [givenTree]->selectedRole = STP_PORT_ROLE_DISABLED;
}

// ============================================================================
// 13.28.a) - 13.28.1
// TRUE, if and only if, agree is TRUE for the given port for all SPTs.
bool allSptAgree (const STP_BRIDGE* bridge)
{
	assert(false); return false; // not implemented
}

// ============================================================================
// 13.28.b) - 13.28.2
// The condition allSynced is TRUE for a given port, for a given tree, if and only if
//
// a) For all ports for the given tree, selected is TRUE, the port's role is the same as its selectedRole, and
//    updtInfo is FALSE; and
// b) The role of the given port is
//    1) Root Port or Alternate Port and synced is TRUE for all ports for the given tree other than the
//       Root Port; or
//    2) Designated Port and synced is TRUE for all ports for the given tree other than the given port; or
//    3) Designated Port, and the tree is an SPT or the IST, and the Root Port of the tree and the given
//       port are both within the Bridge’s SPT Region, and both learning and forwarding are FALSE for
//       the given port; or
//    4) Master Port and synced is TRUE for all ports for the given tree other than the given port.
bool allSynced (const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// a) For all ports for the given tree, selected is TRUE, the port's role is the same as its selectedRole, and updtInfo is FALSE; and
	for (PortIndex portIndex = (PortIndex)0; portIndex < bridge->portCount; portIndex++)
	{
		const PORT_TREE* portTree = bridge->ports[portIndex]->trees[givenTree];

		if (portTree->selected == false)
			return false;

		if (portTree->role != portTree->selectedRole)
			return false;

		if (portTree->updtInfo)
			return false;
	}

	// Condition b) 3) not yet implemented
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP);

	// b) The role of the given Port is
	STP_PORT_ROLE role = bridge->ports[givenPort]->trees[givenTree]->role;
	if ((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_ALTERNATE) || (role == STP_PORT_ROLE_BACKUP))
	{
		// Note AG: The standard doesn tell about the BackupPort role. If we follow the letter of the standard, we should
		// return False when this function is invoked on a Backup port. I tested that and it leads to some weird behavior
		// even in the simple scenario of wiring two ports of the same bridge together: the non-Backup port becomes operEdge!
		// Also in a scenario with multiple bridges and wires we get weird behavior, with the non-Backup port going through
		// a Learning stage sometimes but not always.
		//
		// So I'm inclined to believe we should treat a Backup port the same as an Alternate port.

		// 1) Root Port or Alternate Port and synced is TRUE for all ports for the given tree other than the Root Port; or
		for (PortIndex portIndex = (PortIndex)0; portIndex < bridge->portCount; portIndex++)
		{
			const PORT_TREE* portTree = bridge->ports[portIndex]->trees[givenTree];

			if (portTree->role == STP_PORT_ROLE_ROOT)
				continue;

			if (portTree->synced == false)
				return false;
		}

		return true;
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) || (role == STP_PORT_ROLE_MASTER))
	{
		// 2) Designated Port and synced is TRUE for all ports for the given tree other than the given port; or
		// 4) Master Port     and synced is TRUE for all ports for the given tree other than the given port.
		for (PortIndex portIndex = (PortIndex)0; portIndex < bridge->portCount; portIndex++)
		{
			if (portIndex == (unsigned int) givenPort)
				continue;

			const PORT_TREE* portTree = bridge->ports [portIndex]->trees [givenTree];

//LOG (bridge, givenPort, givenTree, "552: Port {D} synced={D}\r\n", portIndex, portTree->synced);

			if (portTree->synced == false)
				return false;
		}

		return true;
	}
	else
	{
		assert (false);
		return false;
	}
}

// ============================================================================
// 13.28.3
// TRUE, if and only if, for the given port for all trees
// a) selected is TRUE; and
// b) updtInfo is FALSE.
bool allTransmitReady (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	PORT* port = bridge->ports[givenPort];
	for (unsigned int ti = 0; ti < bridge->treeCount(); ti++)
	{
		PORT_TREE* tree = port->trees[ti];
		if (!tree->selected || tree->updtInfo)
			return false;
	}

	return true;
}

// ============================================================================
// 13.28.4
const PRIORITY_VECTOR& BestAgreementPriority()
{
	static const PRIORITY_VECTOR best = { };
	return best;
}

// ============================================================================
// 13.28.5
// TRUE only for CIST state machines; i.e., FALSE for MSTI state machine instances.
bool cist (const STP_BRIDGE* bridge, TreeIndex givenTree)
{
	return givenTree == 0;
}

// ============================================================================
// 13.28.6
// TRUE if the CIST role for the given port is RootPort.
bool cistRootPort (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->role == STP_PORT_ROLE_ROOT;
}

// ============================================================================
// 13.28.7
// TRUE if the CIST role for the given port is DesignatedPort.
bool cistDesignatedPort	(const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->role == STP_PORT_ROLE_DESIGNATED;
}

// ============================================================================
// 13.28.8
// Returns the value of MigrateTime if operPointToPointMAC is TRUE, and the value of MaxAge otherwise.
unsigned short EdgeDelay (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	PORT* port = bridge->ports[givenPort];
	return port->operPointToPointMAC ? bridge->MigrateTime : MaxAge(bridge, givenPort);
}

// ============================================================================
// 13.28.9
// Returns the value of HelloTime if sendRSTP is TRUE, and the value of FwdDelay otherwise.
unsigned short forwardDelay (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	if (bridge->ports[givenPort]->sendRSTP)
		return HelloTime (bridge, givenPort);
	else
		return FwdDelay (bridge, givenPort);
}

// ============================================================================
// 13.28.10
// The Forward Delay component of the CIST's designatedTimes parameter (13.27.21).
unsigned short FwdDelay (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->designatedTimes.ForwardDelay;
}

// ============================================================================
// 13.28.11
// The Hello Time component of the CIST's portTimes parameter (13.27.48) with the recommended default
// value given in Table 13-5.
unsigned short HelloTime (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->portTimes.HelloTime;
}

// ============================================================================
// 13.28.12
// The Max Age component of the CIST's designatedTimes parameter (13.27.21).
unsigned short MaxAge (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->designatedTimes.MaxAge;
}

// ============================================================================
// 13.28.13
// TRUE only for MSTI state machines; i.e., FALSE for CIST or SPT state machine instances
bool msti (const STP_BRIDGE* bridge, TreeIndex treeIndex)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT
	return treeIndex > CIST_INDEX;
}

// ============================================================================
// 13.28.14
// TRUE if the role for any MSTI for the given port is either:
// a) DesignatedPort; or
// b) RootPort, and the instance for the given MSTI and port of the tcWhile timer is not zero.
bool mstiDesignatedOrTCpropagatingRootPort (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT

	for (unsigned int mstiIndex = 0; mstiIndex < bridge->mstiCount; mstiIndex++)
	{
		PORT_TREE* mstiInstance = bridge->ports[givenPort]->trees[1 + mstiIndex];

		if (mstiInstance->role == STP_PORT_ROLE_DESIGNATED)
			return true;

		if ((mstiInstance->role == STP_PORT_ROLE_ROOT) && (mstiInstance->tcWhile != 0))
			return true;
	}

	return false;
}

// ============================================================================
// 13.28.15
// TRUE if the role for any MSTI for the given port is MasterPort.
bool mstiMasterPort (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT

	PORT* port = bridge->ports[givenPort];
	for (unsigned int mstiIndex = 0; mstiIndex < bridge->mstiCount; mstiIndex++)
	{
		if (port->trees [1 + mstiIndex]->role == STP_PORT_ROLE_MASTER)
			return true;
	}

	return false;
}

// ============================================================================
// 13.28.16
// TRUE if operPointToPointMAC (IEEE Std 802.1AC) is TRUE for the Bridge Port.
bool operPointToPoint (const STP_BRIDGE* bridge, PortIndex portIndex)
{
	return bridge->ports[portIndex]->operPointToPointMAC;
}

// ============================================================================
// 13.28.17
// TRUE for a given port if rcvdMsg is TRUE for the CIST or any MSTI for that port.
bool rcvdAnyMsg (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT

	PORT* port = bridge->ports [givenPort];
	for (unsigned int treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
	{
		if (port->trees[treeIndex]->rcvdMsg)
			return true;
	}

	return false;
}

// ============================================================================
// 13.28.18
// TRUE for a given port if and only if rcvdMsg is TRUE for the CIST for that port.
bool rcvdCistMsg (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->rcvdMsg;
}

// ============================================================================
// 13.26.17
// TRUE for a given port and MSTI if and only if rcvdMsg is FALSE for the CIST for that port and rcvdMsg is
// TRUE for the MSTI for that port.
bool rcvdMstiMsg (const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT
	assert (givenTree != CIST_INDEX); // this must be invoked on MSTIs only

	PORT* port = bridge->ports[givenPort];
	return !port->trees[CIST_INDEX]->rcvdMsg && port->trees[givenTree]->rcvdMsg;
}

// ============================================================================
// 13.28.20
// TRUE if the rrWhile timer is clear (zero) for all Ports for the given tree other than the given Port.
bool reRooted (const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		if (portIndex == givenPort)
			continue;

		if (bridge->ports[portIndex]->trees[givenTree]->rrWhile != 0)
			return false;
	}

	return true;
}

// ============================================================================
// 13.28.21
// TRUE if ForceProtocolVersion (13.7.2) is greater than or equal to 2.
bool rstpVersion (const STP_BRIDGE* bridge)
{
	return bridge->ForceProtocolVersion >= 2;
}

// ============================================================================
// 13.28.22
// TRUE only for SPT state machines, in an SPT Bridge; i.e., FALSE for CIST and MSTI state machine instances.
bool spt (const STP_BRIDGE* bridge)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT
	return false;
}

// ============================================================================
// 13.28.23
// TRUE if Force Protocol Version (13.7.2) is less than 2.
bool stpVersion (const STP_BRIDGE* bridge)
{
	return bridge->ForceProtocolVersion < 2;
}

// ============================================================================
// 13.28.24
// TRUE for a given port if and only if updtInfo is TRUE for the CIST for that port.
bool updtCistInfo (const STP_BRIDGE* bridge, PortIndex givenPort)
{
	return bridge->ports[givenPort]->trees[CIST_INDEX]->updtInfo;
}

// ============================================================================
// 13.28.25
// TRUE for a given port and MSTI if and only if updtInfo is TRUE for the MSTI for that port or updtInfo is
// TRUE for the CIST for that port.
//
// NOTE—The dependency of rcvdMstiMsg and updtMstiInfo on CIST variables for the port reflects the fact that MSTIs
// exist in a context of CST parameters. The state machines ensure that the CIST parameters from received BPDUs are
// processed and updated prior to processing MSTI information.
bool updtMstiInfo (const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT
	assert (givenTree != CIST_INDEX); // this must be invoked on MSTIs only

	return bridge->ports[givenPort]->trees[givenTree]->updtInfo || bridge->ports[givenPort]->trees[CIST_INDEX]->updtInfo;
}

// ============================================================================
// Note AG: added by me.
bool rcvdXstMsg	(const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT

	if (givenTree == CIST_INDEX)
		return rcvdCistMsg (bridge, givenPort);
	else
		return rcvdMstiMsg (bridge, givenPort, givenTree);
}

// ============================================================================
// Note AG: added by me.
bool updtXstInfo (const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	assert (bridge->ForceProtocolVersion <= STP_VERSION_MSTP); // not yet implemented for SPT

	return (givenTree == CIST_INDEX) ? updtCistInfo (bridge, givenPort) : updtMstiInfo (bridge, givenPort, givenTree);
}

// ============================================================================

// Aside from the topology changes occurring locally, the application also needs to know about topology changes
// occurring in distant bridges, because, after such a a distant topology change, some MAC address might
// be reachable through a different local port.
//
// When a distant topology change occurs, all bridges in the network will get a notification about it.
// When such a notification is received, the bridge should either:
//   1. clear the filtering databases (=learning tables) for all ports, or
//   2. quickly age-out the filtering databases.
// Cisco recomments the second approach, which is an optimization that reduces the broadcast traffic somewhat.
//
// This _is_ a matter of reliability. Fortunately, it's also simple to get this right:
// just hook into the entry block of the NOTIFIED_TC state in the Topology Change state machine,
// and call the "onNotifiedTopologyChange callback". This covers for topology changes originating in
// distant bridges, while the "flushFdb" callback covers for topology changes originating in this bridge.
//
// Note that the code in setTcFlags() takes care of propagating to MSTIs the topology change flags of the CIST.
// So we don't have to take here special precautions for MSTIs.

// Function not from the the standard. It is meant to be called from the entry block of NOTIFIED_TC.
void CallNotifiedTcCallback (STP_BRIDGE* bridge, TreeIndex treeIndex, unsigned int timestamp)
{
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		PORT* port = bridge->ports [portIndex];
		if (!port->operEdge)
		{
			bridge->callbacks.onNotifiedTopologyChange (bridge, portIndex, treeIndex, timestamp);
		}
	}
}

