
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "../stp.h"
#include "stp_bridge.h"
#include "stp_log.h"
#include "stp_md5.h"
#include <string.h>

static void RunStateMachines (STP_BRIDGE* bridge, unsigned int timestamp);
static void RestartStateMachines (STP_BRIDGE* bridge, unsigned int timestamp);
static void RecomputePrioritiesAndPortRoles (STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp);
static void ComputeMstConfigDigest (STP_BRIDGE* bridge);

// ============================================================================

STP_BRIDGE* STP_CreateBridge (unsigned int portCount,
							  unsigned int mstiCount,
							  unsigned int maxVlanNumber,
							  const STP_CALLBACKS* callbacks,
							  const unsigned char bridgeAddress[6],
							  unsigned int debugLogBufferSize)
{
	// Let's make a few checks on the data types, because we might be compiled with strange
	// compiler options which will turn upside down all our assumptions about structure layouts.
	static_assert (sizeof(unsigned char) == 1, "");
	static_assert (sizeof(unsigned short) == 2, "");
	static_assert (sizeof(unsigned int) == 4, "");
	static_assert (sizeof(INV_UINT2) == 2, "");
	static_assert (sizeof(INV_UINT4) == 4, "");
	static_assert (sizeof(STP_BRIDGE_ADDRESS) == 6, "");
	static_assert (sizeof(BRIDGE_ID) == 8, "");
	static_assert (sizeof(PORT_ID) == 2, "");
	static_assert (sizeof(PRIORITY_VECTOR) == 34, "");
	static_assert (sizeof(MSTP_BPDU) == 102, "");

	// Upper limit for number of MSTIs is defined in 802.1Q-2011, page 342, top paragraph:
	//		"No more than 64 MSTI Configuration Messages shall be encoded in an MST
	//		BPDU, and no more than 64 MSTIs shall be supported by an MST Bridge."
	assert (mstiCount <= 64);

	// As specified in 12.3.i) in 802.1Q-2011, valid port numbers are 1..4095, so our valid port indexes will be 0..4094.
	// This means a maximum of 4095 ports.
	assert ((portCount >= 1) && (portCount < 4096));

	assert (maxVlanNumber <= 4094);

	STP_BRIDGE* bridge = (STP_BRIDGE*) callbacks->allocAndZeroMemory (sizeof (STP_BRIDGE));
	assert (bridge != NULL);

	// See "13.6.2 Force Protocol Version" on page 332
	bridge->ForceProtocolVersion = STP_VERSION_RSTP;
	bridge->TxHoldCount = 6;
	bridge->callbacks = *callbacks;
	bridge->portCount = portCount;
	bridge->mstiCount = mstiCount;
	bridge->maxVlanNumber = maxVlanNumber;

#if STP_USE_LOG
	assert (debugLogBufferSize >= 2); // one byte for the data, one for the null terminator of the string passed to the callback
	bridge->logBuffer = (char*) callbacks->allocAndZeroMemory (debugLogBufferSize);
	assert (bridge->logBuffer != NULL);
	bridge->logBufferMaxSize = debugLogBufferSize;
	bridge->logBufferUsedSize = 0;
	bridge->logCurrentPort = -1;
	bridge->logCurrentTree = -1;
#endif

	// ------------------------------------------------------------------------

	bridge->trees = (BRIDGE_TREE**) callbacks->allocAndZeroMemory ((1 + bridge->mstiCount) * sizeof (BRIDGE_TREE*));
	assert (bridge->trees != NULL);

	bridge->ports = (PORT**) callbacks->allocAndZeroMemory (portCount * sizeof (PORT*));
	assert (bridge->ports != NULL);

	// per-bridge CIST vars
	bridge->trees [CIST_INDEX] = (BRIDGE_TREE*) callbacks->allocAndZeroMemory (sizeof (BRIDGE_TREE));
	assert (bridge->trees [CIST_INDEX] != NULL);
	bridge->trees [CIST_INDEX]->SetBridgeIdentifier (0x8000, CIST_INDEX, bridgeAddress);
	// 13.24.3 in 802.1Q-2011
	// Defaults from Table 13-5 on page 510 in 802.1Q-2018
	bridge->trees [CIST_INDEX]->BridgeTimes.HelloTime     = 2;
	bridge->trees [CIST_INDEX]->BridgeTimes.remainingHops = 20;
	bridge->trees [CIST_INDEX]->BridgeTimes.ForwardDelay  = 15;
	bridge->trees [CIST_INDEX]->BridgeTimes.MaxAge        = 20;

	// per-bridge MSTI vars
	for (unsigned int treeIndex = 1; treeIndex < (1 + bridge->mstiCount); treeIndex++)
	{
		bridge->trees [treeIndex] = (BRIDGE_TREE*) callbacks->allocAndZeroMemory (sizeof (BRIDGE_TREE));
		assert (bridge->trees [treeIndex] != NULL);
		bridge->trees [treeIndex]->SetBridgeIdentifier (0x8000, treeIndex, bridgeAddress);
		bridge->trees [treeIndex]->BridgeTimes.remainingHops = 20;
	}

	// per-port vars
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		bridge->ports [portIndex] = (PORT*) callbacks->allocAndZeroMemory (sizeof (PORT));
		assert (bridge->ports [portIndex] != NULL);

		PORT* port = bridge->ports [portIndex];

		port->trees = (PORT_TREE**) callbacks->allocAndZeroMemory ((1 + bridge->mstiCount) * sizeof (PORT_TREE*));
		assert (port->trees != NULL);

		// per-port CIST and MSTI vars
		for (unsigned int treeIndex = 0; treeIndex < (1 + bridge->mstiCount); treeIndex++)
		{
			port->trees[treeIndex] = (PORT_TREE*) callbacks->allocAndZeroMemory (sizeof (PORT_TREE));
			assert (port->trees[treeIndex] != NULL);
			port->trees[treeIndex]->portId.Set (0x80, (unsigned short) portIndex + 1);
			port->trees[treeIndex]->portTimes = bridge->trees[treeIndex]->BridgeTimes;
			port->trees[treeIndex]->InternalPortPathCost = 200000; // TODO: remove this hardcoded value and calculate the actual value in STP_OnPortEnabled
			port->trees[treeIndex]->pseudoRootId = bridge->trees[treeIndex]->GetBridgeIdentifier();
		}

		port->adminPointToPointMAC = STP_ADMIN_P2P_AUTO;
		port->AutoEdge = true;
		port->enableBPDUrx = true;
		port->enableBPDUtx = true;
	}

	bridge->receivedBpduContent = NULL; // see comment at declaration of receivedBpduContent

	// These were already zeroed by the allocation routine.
	//bridge->MstConfigId.ConfigurationIdentifierFormatSelector = 0;
	//bridge->MstConfigId.RevisionLevel = 0;

	// Let's set a default name for the MST Config.
	STP_GetDefaultMstConfigName (bridgeAddress, bridge->MstConfigId.ConfigurationName);

	bridge->mstConfigTable = (INV_UINT2*) callbacks->allocAndZeroMemory ((1 + maxVlanNumber) * 2);
	assert (bridge->mstConfigTable != NULL);

	// The config table is all zeroes now, so all VIDs map to the CIST, no VID mapped to any MSTI.
	ComputeMstConfigDigest (bridge);

	return bridge;
}

// ============================================================================

void STP_DestroyBridge (STP_BRIDGE* bridge)
{
	bridge->callbacks.freeMemory (bridge->mstConfigTable);

	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
	{
		for (unsigned int treeIndex = 0; treeIndex < (1 + bridge->mstiCount); treeIndex++)
			bridge->callbacks.freeMemory (bridge->ports [portIndex]->trees [treeIndex]);

		bridge->callbacks.freeMemory (bridge->ports [portIndex]->trees);

		bridge->callbacks.freeMemory (bridge->ports [portIndex]);
	}

	for (unsigned int treeIndex = 0; treeIndex < (1 + bridge->mstiCount); treeIndex++)
		bridge->callbacks.freeMemory (bridge->trees [treeIndex]);

	bridge->callbacks.freeMemory (bridge->ports);
	bridge->callbacks.freeMemory (bridge->trees);
#if STP_USE_LOG
	bridge->callbacks.freeMemory (bridge->logBuffer);
#endif
	bridge->callbacks.freeMemory (bridge);
}

// ============================================================================

void STP_StartBridge (STP_BRIDGE* bridge, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Starting the bridge...\r\n", timestamp);

	assert (bridge->started == false);

	bridge->callbacks.enableBpduTrapping (bridge, true, timestamp);

	bridge->started = true;

	RestartStateMachines(bridge, timestamp);

	LOG (bridge, -1, -1, "Bridge started.\r\n");
	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

// ============================================================================

void STP_StopBridge (STP_BRIDGE* bridge, unsigned int timestamp)
{
	assert (bridge->started);
	bridge->started = false;

	bridge->callbacks.enableBpduTrapping (bridge, false, timestamp);

	LOG (bridge, -1, -1, "{T}: Bridge stopped.\r\n", timestamp);
	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

// ============================================================================

void STP_SetBridgeAddress (STP_BRIDGE* bridge, const unsigned char* address, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Setting bridge MAC address to {BA}...", timestamp, address);

	const unsigned char* currentAddress = bridge->trees[CIST_INDEX]->GetBridgeIdentifier().GetAddress().bytes;
	if (memcmp (currentAddress, address, 6) == 0)
	{
		LOG (bridge, -1, -1, " nothing changed.\r\n");
	}
	else
	{
		LOG (bridge, -1, -1, "\r\n");

		for (unsigned int treeIndex = 0; treeIndex < (1 + bridge->mstiCount); treeIndex++)
		{
			// change the MAC address without changing the priority
			BRIDGE_ID bid = bridge->trees[treeIndex]->GetBridgeIdentifier();
			bid.SetAddress (address);
			bridge->trees[treeIndex]->SetBridgeIdentifier(bid);
		}

		if (bridge->started)
			RecomputePrioritiesAndPortRoles (bridge, CIST_INDEX, timestamp);
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

const struct STP_BRIDGE_ADDRESS* STP_GetBridgeAddress (const struct STP_BRIDGE* bridge)
{
	return &bridge->trees [CIST_INDEX]->GetBridgeIdentifier().GetAddress();
}

// ============================================================================

// Table 13-4 on page 348 of 802.1Q-2011
static unsigned int GetDefaultPortPathCost (unsigned int speedMegabitsPerSecond)
{
	if (speedMegabitsPerSecond == 0)
		return 200000000;
	else if (speedMegabitsPerSecond <= 1)
		return 20000000;
	else if (speedMegabitsPerSecond <= 10)
		return 2000000;
	else if (speedMegabitsPerSecond <= 100)
		return 200000;
	else if (speedMegabitsPerSecond <= 1000)
		return 20000;
	else if (speedMegabitsPerSecond <= 10000)
		return 2000;
	else if (speedMegabitsPerSecond <= 100000)
		return 200;
	else if (speedMegabitsPerSecond <= 1000000)
		return 20;
	else
		return 2;
}

void STP_OnPortEnabled (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int speedMegabitsPerSecond, bool detectedPointToPointMAC, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Port {D} good\r\n", timestamp, 1 + portIndex);

	PORT* port = bridge->ports [portIndex];

	assert (!port->portEnabled);
	port->portEnabled = true;

	port->detectedPointToPointMAC = detectedPointToPointMAC;
	if (port->adminPointToPointMAC != STP_ADMIN_P2P_AUTO)
		port->operPointToPointMAC = (port->adminPointToPointMAC == STP_ADMIN_P2P_FORCE_TRUE);
	else
		port->operPointToPointMAC = detectedPointToPointMAC;

	port->detectedPortPathCost = GetDefaultPortPathCost(speedMegabitsPerSecond);
	if (port->adminExternalPortPathCost != 0)
		port->ExternalPortPathCost = port->adminExternalPortPathCost;
	else
		port->ExternalPortPathCost = port->detectedPortPathCost;

// TODO: calculate also InternalPortPathCost and remove its hardcoded value from STP_CreateBridge

	if (bridge->started)
		RunStateMachines (bridge, timestamp);

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

// ============================================================================

void STP_OnPortDisabled (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Port {D} down\r\n", timestamp, 1 + portIndex);

	PORT* port = bridge->ports[portIndex];
	// We allow calling this function on an already disabled port.
	if (port->portEnabled)
	{
		port->detectedPointToPointMAC = false;
		port->operPointToPointMAC = false;
		port->detectedPortPathCost = 0;
		port->ExternalPortPathCost = 0;
		// TODO: clear also InternalPortPathCost

		port->portEnabled = false;

		if (bridge->started)
			RunStateMachines (bridge, timestamp);
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

// ============================================================================

void STP_OnOneSecondTick (STP_BRIDGE* bridge, unsigned int timestamp)
{
	if (bridge->started)
	{
		LOG (bridge, -1, -1, "{T}: One second:\r\n", timestamp);

		for (unsigned int givenPort = 0; givenPort < bridge->portCount; givenPort++)
			bridge->ports [givenPort]->tick = true;

		RunStateMachines (bridge, timestamp);

		LOG (bridge, -1, -1, "------------------------------------\r\n");
		FLUSH_LOG (bridge);
	}
}

// ============================================================================

void STP_OnBpduReceived (STP_BRIDGE* bridge, unsigned int portIndex, const unsigned char* bpdu, unsigned int bpduSize, unsigned int timestamp)
{
	if (bridge->started)
	{
		if (bridge->ports [portIndex]->portEnabled == false)
		{
			LOG (bridge, -1, -1, "{T}: WARNING: BPDU received on disabled port {D}. The STP library is discarding it.\r\n", timestamp, 1 + portIndex);
		}
		else
		{
			LOG (bridge, -1, -1, "{T}: BPDU received on Port {D}:\r\n", timestamp, 1 + portIndex);

			enum VALIDATED_BPDU_TYPE type = STP_GetValidatedBpduType (bpdu, bpduSize);
			bool passToStateMachines;
			switch (type)
			{
				case VALIDATED_BPDU_TYPE_STP_CONFIG:
					#if STP_USE_LOG
						LOG (bridge, portIndex, -1, "Config BPDU:\r\n");
						LOG_INDENT (bridge);
						DumpConfigBpdu (bridge, portIndex, -1, (const MSTP_BPDU*) bpdu);
						LOG_UNINDENT (bridge);
					#endif
					passToStateMachines = true;
					break;

				case VALIDATED_BPDU_TYPE_RST:
					#if STP_USE_LOG
						LOG (bridge, portIndex, -1, "RSTP BPDU:\r\n");
						LOG_INDENT (bridge);
						DumpRstpBpdu (bridge, portIndex, -1, (const MSTP_BPDU*) bpdu);
						LOG_UNINDENT (bridge);
					#endif
					passToStateMachines = true;
					break;

				case VALIDATED_BPDU_TYPE_MST:
					#if STP_USE_LOG
						LOG (bridge, portIndex, -1, "MSTP BPDU:\r\n");
						LOG_INDENT (bridge);
						DumpMstpBpdu (bridge, portIndex, -1, (const MSTP_BPDU*) bpdu);
						LOG_UNINDENT (bridge);
					#endif
					passToStateMachines = true;
					break;

				case VALIDATED_BPDU_TYPE_STP_TCN:
					LOG (bridge, portIndex, -1, "TCN BPDU.\r\n");
					passToStateMachines = true;
					break;

				default:
					LOG (bridge, portIndex, -1, "Invalid BPDU received. Discarding it.\r\n");
					passToStateMachines = false;
			}

			if (passToStateMachines)
			{
				assert (bridge->receivedBpduContent == NULL);
				assert (bridge->receivedBpduType == VALIDATED_BPDU_TYPE_UNKNOWN);
				assert (bridge->ports [portIndex]->rcvdBpdu == false);

				bridge->receivedBpduContent = (MSTP_BPDU*) bpdu;
				bridge->receivedBpduType = type;
				bridge->ports [portIndex]->rcvdBpdu = true;

				RunStateMachines (bridge, timestamp);

				bridge->receivedBpduContent = NULL; // to cause an exception on access
				bridge->receivedBpduType = VALIDATED_BPDU_TYPE_UNKNOWN; // to cause asserts on access

				// Check that the state machines did process the BPDU.
				assert (bridge->ports [portIndex]->rcvdBpdu == false);
			}
		}

		LOG (bridge, -1, -1, "------------------------------------\r\n");
		FLUSH_LOG (bridge);
	}
}

// ============================================================================

bool STP_IsBridgeStarted (const STP_BRIDGE* bridge)
{
	return bridge->started;
}

// ============================================================================

void STP_EnableLogging (STP_BRIDGE* bridge, bool enable)
{
	#if STP_USE_LOG
		bridge->loggingEnabled = enable;
	#endif
}

// ============================================================================

bool STP_IsLoggingEnabled (const STP_BRIDGE* bridge)
{
	#if STP_USE_LOG
		return bridge->loggingEnabled;
	#else
		return false;
	#endif
}

// ============================================================================

#if STP_USE_LOG
template<typename PortTreeArgs>
static void LogTransition (STP_BRIDGE* bridge, const char* smName, const char* newStateName, PortTreeArgs args);

template<>
void LogTransition (STP_BRIDGE* bridge, const char* smName, const char* newStateName, TreeIndex ti)
{
	LOG (bridge, -1, ti, "Bridge: ");
	if (bridge->ForceProtocolVersion >= STP_VERSION_MSTP)
	{
		if (ti == CIST_INDEX)
			LOG (bridge, -1, ti, "CIST: ");
		else
			LOG (bridge, -1, ti, "MST{D}: ", ti);
	}

	LOG (bridge, -1, ti, "{S}: -> {S}\r\n", smName, newStateName);
}

template<>
void LogTransition (STP_BRIDGE* bridge, const char* smName, const char* newStateName, PortIndex pi)
{
	LOG (bridge, pi, -1, "Port {D}: ", 1 + pi);
	LOG (bridge, pi, -1, "{S}: -> {S}\r\n", smName, newStateName);
}

template<>
void LogTransition (STP_BRIDGE* bridge, const char* smName, const char* newStateName, PortAndTree pt)
{
	PortIndex pi = pt.portIndex;
	TreeIndex ti = pt.treeIndex;
	LOG (bridge, pi, ti, "Port {D}: ", 1 + pi);
	if (bridge->ForceProtocolVersion >= STP_VERSION_MSTP)
	{
		if (ti == CIST_INDEX)
			LOG (bridge, pi, ti, "CIST: ");
		else
			LOG (bridge, pi, ti, "MST{D}: ", ti);
	}
	LOG (bridge, pi, ti, "{S}: -> {S}\r\n", smName, newStateName);
}
#endif

// ============================================================================

template<typename State, typename PortTreeArgs>
static bool RunStateMachineInstance (STP_BRIDGE* bridge, const StateMachine<State, PortTreeArgs>& smInfo, State& state, unsigned int timestamp, PortTreeArgs portTreeArgs)
{
	volatile bool changed = false;

rep:
	State newState = smInfo.checkConditions (bridge, portTreeArgs, state);
	if (newState != 0)
	{
		#if STP_USE_LOG
			const char* newStateName = smInfo.getStateName(newState);
			LogTransition (bridge, smInfo.smName, newStateName, portTreeArgs);
		#endif

		smInfo.initState (bridge, portTreeArgs, newState, timestamp);

		state = newState;
		changed = true;
		goto rep;
	}

	return changed;
}

// ============================================================================

static void RunStateMachines (STP_BRIDGE* bridge, unsigned int timestamp)
{
	bool changed;

	do
	{
		changed = false;

		for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		{
			PORT* port = bridge->ports[portIndex];
			changed |= RunStateMachineInstance (bridge, PortTimers           ::sm, port->portTimersState,            timestamp, (PortIndex) portIndex);
			changed |= RunStateMachineInstance (bridge, PortProtocolMigration::sm, port->portProtocolMigrationState, timestamp, (PortIndex) portIndex);
			changed |= RunStateMachineInstance (bridge, PortReceive          ::sm, port->portReceiveState,           timestamp, (PortIndex) portIndex);
			changed |= RunStateMachineInstance (bridge, BridgeDetection      ::sm, port->bridgeDetectionState,       timestamp, (PortIndex) portIndex);
			//changed |= RunStateMachineInstance (bridge, &L2GP::sm,                  portIndex, -1, &port->l2gpState,                  timestamp);

			for (unsigned int treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
			{
				PORT_TREE* tree = port->trees[treeIndex];
				PortAndTree pt = { (PortIndex)portIndex, (TreeIndex)treeIndex };
				changed |= RunStateMachineInstance (bridge, PortInformation    ::sm, tree->portInformationState,     timestamp, pt);
				changed |= RunStateMachineInstance (bridge, PortRoleTransitions::sm, tree->portRoleTransitionsState, timestamp, pt);
				changed |= RunStateMachineInstance (bridge, PortStateTransition::sm, tree->portStateTransitionState, timestamp, pt);
				changed |= RunStateMachineInstance (bridge, TopologyChange     ::sm, tree->topologyChangeState,      timestamp, pt);
			}
		}

		for (unsigned int treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
		{
			BRIDGE_TREE* tree = bridge->trees[treeIndex];
			changed |= RunStateMachineInstance (bridge, PortRoleSelection::sm, tree->portRoleSelectionState, timestamp, (TreeIndex) treeIndex);
		}

		// We execute the PortTransmit state machine only after all other state machines have finished executing,
		// so as to avoid transmitting BPDUs containing results from intermediary calculations.
		// See Note 1 on page 541 of 802.1Q-2018.
		if (!changed)
		{
			for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
			{
				PORT* port = bridge->ports[portIndex];
				changed |= RunStateMachineInstance (bridge, PortTransmit::sm, port->portTransmitState, timestamp, (PortIndex) portIndex);
			}
		}
	} while (changed);
}

static void RestartStateMachines (STP_BRIDGE* bridge, unsigned int timestamp)
{
	bridge->BEGIN = true;
	RunStateMachines (bridge, timestamp);
	bridge->BEGIN = false;
	RunStateMachines (bridge, timestamp);
}

// ============================================================================

void STP_SetPortAdminEdge (struct STP_BRIDGE* bridge, unsigned int portIndex, bool adminEdge, unsigned int timestamp)
{
	bridge->ports [portIndex]->AdminEdge = adminEdge;
}

bool STP_GetPortAdminEdge (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports [portIndex]->AdminEdge;
}

// ============================================================================

void STP_SetPortAutoEdge (struct STP_BRIDGE* bridge, unsigned int portIndex, bool autoEdge, unsigned int timestamp)
{
	bridge->ports [portIndex]->AutoEdge = autoEdge;
}

bool STP_GetPortAutoEdge (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports [portIndex]->AutoEdge;
}

// ============================================================================

void STP_SetAdminPointToPointMAC (struct STP_BRIDGE* bridge, unsigned int portIndex, enum STP_ADMIN_P2P adminPointToPointMAC, unsigned int timestamp)
{
	const char* p2pString = STP_GetAdminP2PString (adminPointToPointMAC);
	LOG (bridge, portIndex, -1, "{T}: Setting adminPointToPointMAC = {S} on port {D}...\r\n", timestamp, p2pString, 1 + portIndex);

	PORT* port = bridge->ports[portIndex];

	port->adminPointToPointMAC = adminPointToPointMAC;

	if (port->portEnabled)
	{
		bool newOperPointToPointMAC = (adminPointToPointMAC == STP_ADMIN_P2P_FORCE_TRUE)
			|| ((adminPointToPointMAC == STP_ADMIN_P2P_AUTO) && port->detectedPointToPointMAC);

		if (port->operPointToPointMAC != newOperPointToPointMAC)
		{
			port->operPointToPointMAC = newOperPointToPointMAC;
			if (bridge->started)
				RunStateMachines (bridge, timestamp);
		}
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

STP_ADMIN_P2P STP_GetAdminPointToPointMAC (const STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports[portIndex]->adminPointToPointMAC;
}

bool STP_GetDetectedPointToPointMAC (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports[portIndex]->detectedPointToPointMAC;
}

bool STP_GetOperPointToPointMAC (const STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports[portIndex]->operPointToPointMAC;
}

// ============================================================================

static void RecomputePrioritiesAndPortRoles (STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	// From page 511 of 802.1Q-2018:
	// BridgeIdentifier, BridgePriority, and BridgeTimes are not modified by the operation of the spanning tree
	// protocols but are treated as constants by the state machines. If they are modified by management, spanning
	// tree priority vectors and Port Role assignments for all trees shall be recomputed, as specified by the
	// operation of the Port Role Selection state machine (13.36) by clearing selected (13.27.67) and setting
	// reselect (13.27.62) for all Bridge Ports for the relevant MSTI and for all trees if the CIST parameter is
	// changed.
	if (treeIndex == CIST_INDEX)
	{
		// Recompute all trees.
		// Note that callers of this function expect recomputation for all trees when CIST_INDEX is passed, so don't change this functionality.
		for (treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
		{
			for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
			{
				PORT_TREE* portTree = bridge->ports[portIndex]->trees[treeIndex];
				portTree->selected = false;
				portTree->reselect = true;
			}
		}
	}
	else
	{
		// recompute specified MSTI
		for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		{
			PORT_TREE* portTree = bridge->ports[portIndex]->trees[treeIndex];
			portTree->selected = false;
			portTree->reselect = true;
		}
	}

	RunStateMachines (bridge, timestamp);
}

// Problem when setting a worse bridge priority (numerically higher)
// on the root bridge, and that bridge remains root even with the new priority:
//
// BPDUs with the old priority could still be propagating throughout the network, until they are discarded
// due to MaxAge / remainingHops. These BPDUs will mess up all priority calculations while propagating,
// because they have the same bridge address, so they will be Superior to the BPDUs newly generated by the same root.
//
// This increases the convergence time by up to HelloTime seconds, and it seems to be a problem of the protocol itself.
// If compounded with delays introduced either by other problems of the protocol, or by problems in the rest of the firmware,
// it might leads to the formation of loops.
//
// I don't think this could be resolved given the current BPDU format. In a future version of the protocol it could be resolved,
// for example, by encoding a timestamp in BPDUs, and using this timestamp to determine whether a received BPDU is Superior.

void STP_SetBridgePriority (STP_BRIDGE* bridge, unsigned int treeIndex, unsigned short bridgePriority, unsigned int timestamp)
{
	// See table 13-3 on page 501 of 802.1Q-2018.

	assert ((bridgePriority & 0x0FFF) == 0);

	assert (treeIndex <= bridge->mstiCount);

	LOG (bridge, -1, -1, "{T}: Setting bridge priority: tree {TN} prio = {D}...\r\n", timestamp, treeIndex, bridgePriority);

	BRIDGE_ID bid = bridge->trees[treeIndex]->GetBridgeIdentifier();
	if (bid.GetPriority() != bridgePriority)
	{
		LOG (bridge, -1, -1, "\r\n");

		bid.SetPriority(bridgePriority, treeIndex);
		bridge->trees[treeIndex]->SetBridgeIdentifier(bid);

		if (bridge->started && (treeIndex < bridge->treeCount()))
			RecomputePrioritiesAndPortRoles (bridge, treeIndex, timestamp);
	}
	else
		LOG (bridge, -1, -1, " nothing changed.\r\n");

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

unsigned short STP_GetBridgePriority (const STP_BRIDGE* bridge, unsigned int treeIndex)
{
	assert (treeIndex <= bridge->mstiCount);

	return bridge->trees [treeIndex]->GetBridgeIdentifier ().GetPriority () & 0xF000;
}

// ============================================================================

void STP_SetPortPriority (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned char portPriority, unsigned int timestamp)
{
	// See table 13-3 on page 348 of 802.1Q-2011
	// See 13.25.32 in 802.1Q-2011

	assert ((portPriority % 16) == 0);
	assert (portIndex < bridge->portCount);
	assert (treeIndex <= bridge->mstiCount);

	LOG (bridge, -1, -1, "{T}: Setting port priority: port {D} tree {TN} prio = {D}...\r\n",
		 timestamp,
		 1 + portIndex,
		 treeIndex,
		 portPriority);

	bridge->ports [portIndex]->trees [treeIndex]->portId.SetPriority (portPriority);

	// It would make sense that stuff is recomputed also when the port priority in the portId variable
	// is changed (as it is recomputed for the bridge priority), but either the spec does not mention this, or I'm not seeing it.
	// Anyway, information about the new port priority can only be propagated by such a recomputation, so let's do that.
	if (bridge->started && (treeIndex < bridge->treeCount()))
		RecomputePrioritiesAndPortRoles (bridge, treeIndex, timestamp);

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

unsigned char STP_GetPortPriority (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex)
{
	// See 13.25.32 in 802.1Q-2011

	assert (portIndex < bridge->portCount);
	assert (treeIndex <= bridge->mstiCount);

	unsigned char priority = bridge->ports [portIndex]->trees [treeIndex]->portId.GetPriority();
	return priority;
}

unsigned short STP_GetPortIdentifier (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex)
{
	assert (portIndex < bridge->portCount);
	assert (treeIndex <= bridge->mstiCount);

	unsigned short id = bridge->ports [portIndex]->trees [treeIndex]->portId.GetPortIdentifier ();
	return id;
}

// ============================================================================

void STP_GetDefaultMstConfigName (const unsigned char bridgeAddress[6], char nameOut[18])
{
	// Cisco uses lowercase letters here; let's do the same.
	char* ptr = nameOut;
	for (unsigned int i = 0; i < 6; i++)
	{
		unsigned int val = bridgeAddress[i] >> 4;
		*ptr++ = (val < 10) ? (val + '0') : (val - 10 + 'a');
		val = bridgeAddress[i] & 0x0F;
		*ptr++ = (val < 10) ? (val + '0') : (val - 10 + 'a');
		*ptr++ = (i < 5) ? ':' : 0;
	}
}

void STP_SetMstConfigName (STP_BRIDGE* bridge, const char* name, unsigned int timestamp)
{
	assert (strlen (name) <= 32);

	LOG (bridge, -1, -1, "{T}: Setting MST Config Name to \"{S}\"...\r\n", timestamp, name);

	memset (bridge->MstConfigId.ConfigurationName, 0, 32);
	memcpy (bridge->MstConfigId.ConfigurationName, name, strlen (name));

	if (bridge->started)
		RestartStateMachines(bridge, timestamp);

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

// ============================================================================

void STP_SetMstConfigRevisionLevel (STP_BRIDGE* bridge, unsigned short revisionLevel, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Setting MST Config Revision Level to {D}...\r\n", timestamp, (int) revisionLevel);

	bridge->MstConfigId.RevisionLevelHigh = revisionLevel >> 8;
	bridge->MstConfigId.RevisionLevelLow = revisionLevel & 0xff;

	if (bridge->started)
		RestartStateMachines(bridge, timestamp);

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

static void ComputeMstConfigDigest (STP_BRIDGE* bridge)
{
	HMAC_MD5_CONTEXT context;
	HMAC_MD5_Init (&context);
	HMAC_MD5_Update (&context, bridge->mstConfigTable, 2 * (1 + bridge->maxVlanNumber));

	unsigned short zero = 0;
	for (int i = (1 + bridge->maxVlanNumber); i < 4096; i++)
		HMAC_MD5_Update (&context, &zero, 2);

	HMAC_MD5_End (&context);

	memcpy (bridge->MstConfigId.ConfigurationDigest, context.digest, 16);
}

void STP_SetMstConfigTable (struct STP_BRIDGE* bridge, const STP_CONFIG_TABLE_ENTRY* entries, unsigned int entryCount, unsigned int timestamp)
{
	assert (entryCount == 1 + bridge->maxVlanNumber);

	LOG (bridge, -1, -1, "{T}: Setting MST Config Table... ", timestamp);

	if (memcmp (bridge->mstConfigTable, entries, entryCount * 2) == 0)
	{
		LOG (bridge, -1, -1, "... nothing changed.\r\n");
	}
	else
	{
		// Check that the caller is not trying to map a VLAN to a too-large tree number.
		assert (entries[0].unused == 0);
		assert (entries[0].treeIndex == 0);
		for (unsigned int vlan = 1; vlan < entryCount; vlan++)
		{
			assert (entries[vlan].unused == 0);
			assert (entries[vlan].treeIndex < (1 + bridge->mstiCount));
		}

		if (entryCount == 4096)
			assert (entries[4095].treeIndex == 0);

		memcpy (bridge->mstConfigTable, entries, entryCount * 2);

		ComputeMstConfigDigest (bridge);

		LOG (bridge, -1, -1, "New digest: 0x{X2}{X2}...{X2}{X2}.\r\n",
			 bridge->MstConfigId.ConfigurationDigest[0], bridge->MstConfigId.ConfigurationDigest[1],
			 bridge->MstConfigId.ConfigurationDigest[14], bridge->MstConfigId.ConfigurationDigest[15]);

		if (bridge->started)
			RestartStateMachines(bridge, timestamp);
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

void STP_SetMstConfigTableEntry (struct STP_BRIDGE* bridge, unsigned int vlanNumber, unsigned int treeIndex, unsigned int timestamp)
{
	assert (vlanNumber <= bridge->maxVlanNumber);

	LOG (bridge, -1, -1, "{T}: Setting MST Config Table... ", timestamp);

	if (bridge->mstConfigTable[vlanNumber].GetValue() == treeIndex)
	{
		LOG (bridge, -1, -1, "... nothing changed.\r\n");
	}
	else
	{
		// Check that the caller is not trying to map a VLAN to a too-large tree number.
		if ((vlanNumber == 0) || (vlanNumber == 4095))
			assert (treeIndex == 0);
		else
			assert (treeIndex < (1 + bridge->mstiCount));

		bridge->mstConfigTable[vlanNumber] = (unsigned short) treeIndex;

		ComputeMstConfigDigest (bridge);

		LOG (bridge, -1, -1, "New digest: 0x{X2}{X2}...{X2}{X2}.\r\n",
			bridge->MstConfigId.ConfigurationDigest[0], bridge->MstConfigId.ConfigurationDigest[1],
			bridge->MstConfigId.ConfigurationDigest[14], bridge->MstConfigId.ConfigurationDigest[15]);

		if (bridge->started)
			RestartStateMachines(bridge, timestamp);
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

const STP_CONFIG_TABLE_ENTRY* STP_GetMstConfigTable (STP_BRIDGE* bridge, unsigned int* entryCountOut)
{
	*entryCountOut = 1 + bridge->maxVlanNumber;
	return (const STP_CONFIG_TABLE_ENTRY*) bridge->mstConfigTable;
}

// ============================================================================

unsigned int STP_GetPortCount (const STP_BRIDGE* bridge)
{
	return bridge->portCount;
}

unsigned int STP_GetMstiCount (const STP_BRIDGE* bridge)
{
	return bridge->mstiCount;
}

enum STP_VERSION STP_GetStpVersion (const STP_BRIDGE* bridge)
{
	return bridge->ForceProtocolVersion;
}

void STP_SetStpVersion (STP_BRIDGE* bridge, enum STP_VERSION version, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Switching to {S}... ", timestamp, STP_GetVersionString(version));

	if (bridge->ForceProtocolVersion == version)
	{
		LOG (bridge, -1, -1, "... bridge was already running {S}.\r\n", STP_GetVersionString(version));
	}
	else
	{
		LOG (bridge, -1, -1, "\r\n");

		bridge->ForceProtocolVersion = version;

		if (bridge->started)
			RestartStateMachines (bridge, timestamp);
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

bool STP_GetPortEnabled (const STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports [portIndex]->portEnabled;
}

STP_PORT_ROLE STP_GetPortRole (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex)
{
	// This value has meaning only while STP is enabled. Let's check that it is.
	assert (bridge->started);
	return bridge->ports [portIndex]->trees [treeIndex]->role;
}

bool STP_GetPortLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex)
{
	// This value has meaning only while STP is enabled. Let's check that it is.
	assert (bridge->started);
	return bridge->ports [portIndex]->trees [treeIndex]->learning;
}

bool STP_GetPortForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex)
{
	// This value has meaning only while STP is enabled. Let's check that it is.
	assert (bridge->started);
	return bridge->ports [portIndex]->trees [treeIndex]->forwarding;
}

bool STP_GetPortOperEdge (const STP_BRIDGE* bridge, unsigned int portIndex)
{
	// This value has meaning only while STP is enabled. Let's check that it is.
	assert (bridge->started);
	return bridge->ports [portIndex]->operEdge;
}

unsigned int STP_GetMaxVlanNumber (const STP_BRIDGE* bridge)
{
	return bridge->maxVlanNumber;
}

unsigned int STP_GetTreeIndexFromVlanNumber (const STP_BRIDGE* bridge, unsigned int vlanNumber)
{
	assert (vlanNumber <= bridge->maxVlanNumber);

	switch (bridge->ForceProtocolVersion)
	{
		case STP_VERSION_LEGACY_STP:
		case STP_VERSION_RSTP:
			return 0;

		case STP_VERSION_MSTP:
			return bridge->mstConfigTable[vlanNumber].GetValue();

		default:
			assert(false); return 0;
	}
}

const struct STP_MST_CONFIG_ID* STP_GetMstConfigId (const struct STP_BRIDGE* bridge)
{
	return &bridge->MstConfigId;
}

// ============================================================================

const char* STP_GetPortRoleString (STP_PORT_ROLE portRole)
{
	switch (portRole)
	{
		case STP_PORT_ROLE_DISABLED:	return "Disabled";
		case STP_PORT_ROLE_ROOT:		return "Root";
		case STP_PORT_ROLE_DESIGNATED:	return "Designated";
		case STP_PORT_ROLE_ALTERNATE:	return "Alternate";
		case STP_PORT_ROLE_BACKUP:		return "Backup";
		case STP_PORT_ROLE_MASTER:		return "Master";
		default:						return "(unknown)";
	}
}

static const char LegacySTPString[] = "LegacySTP";
static const char RSTPString[] = "RSTP";
static const char MSTPString[] = "MSTP";

const char* STP_GetVersionString (enum STP_VERSION version)
{
	switch (version)
	{
		case STP_VERSION_LEGACY_STP:	return LegacySTPString;
		case STP_VERSION_RSTP:			return RSTPString;
		case STP_VERSION_MSTP:			return MSTPString;
		default:
			assert(false);
			return NULL;
	}
}

enum STP_VERSION STP_GetVersionFromString (const char* str)
{
	if (strcmp(str, LegacySTPString) == 0)
		return STP_VERSION_LEGACY_STP;

	if (strcmp(str, RSTPString) == 0)
		return STP_VERSION_RSTP;

	if (strcmp(str, MSTPString) == 0)
		return STP_VERSION_MSTP;

	assert(false);
	return (STP_VERSION) -1;
}

const char* STP_GetAdminP2PString (enum STP_ADMIN_P2P adminP2P)
{
	switch (adminP2P)
	{
		case STP_ADMIN_P2P_AUTO:        return "Auto";
		case STP_ADMIN_P2P_FORCE_TRUE:  return "ForceTrue";
		case STP_ADMIN_P2P_FORCE_FALSE: return "ForceFalse";
		default: return NULL;
	}
}

// ============================================================================

void STP_GetRootPriorityVector (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned char priorityVectorOut[36])
{
	assert (bridge->started);
	const unsigned char* rootPriority = (const unsigned char*) &bridge->trees [treeIndex]->rootPriority;
	const unsigned char* rootPortId   = (const unsigned char*) &bridge->trees [treeIndex]->rootPortId;
	memcpy (priorityVectorOut, rootPriority, 34);
	priorityVectorOut [34] = rootPortId [0];
	priorityVectorOut [35] = rootPortId [1];
}

// Retrieves the rootTimes variable described in 13.26.1 in 802.1Q-2018.
void STP_GetRootTimes (const STP_BRIDGE* bridge,
					   unsigned int treeIndex,
					   unsigned short* forwardDelayOutOrNull,
					   unsigned short* helloTimeOutOrNull,
					   unsigned short* maxAgeOutOrNull,
					   unsigned short* messageAgeOutOrNull,
					   unsigned char* remainingHopsOutOrNull)
{
	// A MSTI can be specified (as opposed to the CIST) only while running MSTP.
	assert (treeIndex < bridge->treeCount());

	BRIDGE_TREE* tree = bridge->trees [treeIndex];

	if (forwardDelayOutOrNull != NULL)
		*forwardDelayOutOrNull = tree->rootTimes.ForwardDelay;

	if (helloTimeOutOrNull != NULL)
		*helloTimeOutOrNull = tree->rootTimes.HelloTime;

	if (maxAgeOutOrNull != NULL)
		*maxAgeOutOrNull = tree->rootTimes.MaxAge;

	if (messageAgeOutOrNull != NULL)
		*messageAgeOutOrNull = tree->rootTimes.MessageAge;

	if (remainingHopsOutOrNull != NULL)
		*remainingHopsOutOrNull = tree->rootTimes.remainingHops;
}

// ============================================================================

bool STP_IsCistRoot (const STP_BRIDGE* bridge)
{
	assert (bridge->started);
	BRIDGE_TREE* cist = bridge->trees[CIST_INDEX];
	return cist->rootPriority.RootId == cist->GetBridgeIdentifier();
}

bool STP_IsRegionalRoot (const STP_BRIDGE* bridge, unsigned int treeIndex)
{
	assert (bridge->started);
	assert ((treeIndex > 0) && (treeIndex < bridge->treeCount()));
	BRIDGE_TREE* tree = bridge->trees [treeIndex];
	return tree->rootPriority.RegionalRootId == tree->GetBridgeIdentifier();
}

// ============================================================================

void  STP_SetApplicationContext (STP_BRIDGE* bridge, void* applicationContext)
{
	bridge->applicationContext = applicationContext;
}

void* STP_GetApplicationContext (const STP_BRIDGE* bridge)
{
	return bridge->applicationContext;
}

// ============================================================================

void STP_MST_CONFIG_ID::Dump (STP_BRIDGE* bridge, int port, int tree) const
{
	char namesz [33];
	memcpy (namesz, ConfigurationName, 32);
	namesz [32] = 0;
	LOG (bridge, port, tree, "Name=\"{S}\", Rev={D}, Digest={X2}{X2}..{X2}{X2}\r\n",
		 namesz,
		 (RevisionLevelHigh << 8) | RevisionLevelLow,
		 ConfigurationDigest [0], ConfigurationDigest [1], ConfigurationDigest [14], ConfigurationDigest [15]);
}

// ============================================================================

bool STP_MST_CONFIG_ID::operator== (const STP_MST_CONFIG_ID& rhs) const
{
	return memcmp (this, &rhs, sizeof (*this)) == 0;
}

bool STP_MST_CONFIG_ID::operator< (const STP_MST_CONFIG_ID& rhs) const
{
	return memcmp (this, &rhs, sizeof(*this)) < 0;
}

// ============================================================================

void STP_SetAdminExternalPortPathCost (struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int adminExternalPortPathCost, unsigned int timestamp)
{
	LOG (bridge, -1, -1, "{T}: Setting Port {D} AdminExternalPortPathCost to {D}...\r\n", timestamp, 1 + portIndex, adminExternalPortPathCost);

	PORT* port = bridge->ports[portIndex];

	port->adminExternalPortPathCost = adminExternalPortPathCost;

	if (port->portEnabled)
	{
		unsigned int newCost = (port->adminExternalPortPathCost != 0) ? port->adminExternalPortPathCost : port->detectedPortPathCost;
		if (port->ExternalPortPathCost != newCost)
		{
			port->ExternalPortPathCost = newCost;
			LOG (bridge, -1, -1, "  ExternalPortPathCost is now {D}.\r\n", port->ExternalPortPathCost);

			if (bridge->started)
				RecomputePrioritiesAndPortRoles (bridge, CIST_INDEX, timestamp);
		}
	}

	LOG (bridge, -1, -1, "------------------------------------\r\n");
	FLUSH_LOG (bridge);
}

unsigned int STP_GetAdminExternalPortPathCost (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports[portIndex]->adminExternalPortPathCost;
}

unsigned int STP_GetDetectedPortPathCost (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports[portIndex]->detectedPortPathCost;
}

unsigned int STP_GetExternalPortPathCost (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	const PORT* port = bridge->ports[portIndex];
	return port->portEnabled ? port->ExternalPortPathCost : 0;
}

// ============================================================================

void STP_SetBridgeHelloTime (struct STP_BRIDGE* bridge, unsigned int helloTime, unsigned int timestamp)
{
	// Note AG: As of 2011, the standard fixes this to two seconds (Table 13-5 on page 510 in 802.1Q-2018),
	// and it even requires ignoring any HelloTime value received and using two seconds instead (13.29.20 in 802.1Q-2018).
	// I wrote this function only as a placeholder for this comment, so people won't wonder about "missing" functionality.
	assert(helloTime == 2);
}

extern "C" unsigned int STP_GetBridgeHelloTime (const struct STP_BRIDGE* bridge)
{
	return bridge->trees[CIST_INDEX]->BridgeTimes.HelloTime;
}

extern "C" unsigned int STP_GetHelloTime (const struct STP_BRIDGE* bridge)
{
	return bridge->trees[CIST_INDEX]->rootTimes.HelloTime;
}

// ============================================================================

extern "C" void STP_SetBridgeMaxAge (struct STP_BRIDGE* bridge, unsigned int maxAge, unsigned int timestamp)
{
	assert ((maxAge >= 6) && (maxAge <= 40)); // Table 13-5 in 802.1Q-2018

	if (bridge->trees[CIST_INDEX]->BridgeTimes.MaxAge != maxAge)
	{
		bridge->trees[CIST_INDEX]->BridgeTimes.MaxAge = maxAge;
		if (bridge->started)
			RecomputePrioritiesAndPortRoles (bridge, CIST_INDEX, timestamp);
	}
}

extern "C" unsigned int STP_GetBridgeMaxAge (const struct STP_BRIDGE* bridge)
{
	return bridge->trees[CIST_INDEX]->BridgeTimes.MaxAge;
}

extern "C" unsigned int STP_GetMaxAge (const struct STP_BRIDGE* bridge)
{
	return bridge->trees[CIST_INDEX]->rootTimes.MaxAge;
}

// ============================================================================

extern "C" void STP_SetBridgeForwardDelay (struct STP_BRIDGE* bridge, unsigned int forwardDelay, unsigned int timestamp)
{
	assert ((forwardDelay >= 4) && (forwardDelay <= 30)); // Table 13-5 in 802.1Q-2018

	if (bridge->trees[CIST_INDEX]->BridgeTimes.ForwardDelay != forwardDelay)
	{
		bridge->trees[CIST_INDEX]->BridgeTimes.ForwardDelay = forwardDelay;
		if (bridge->started)
			RecomputePrioritiesAndPortRoles (bridge, CIST_INDEX, timestamp);
	}
}

extern "C" unsigned int STP_GetBridgeForwardDelay (const struct STP_BRIDGE* bridge)
{
	return bridge->trees[CIST_INDEX]->BridgeTimes.ForwardDelay;
}

extern "C" unsigned int STP_GetForwardDelay (const struct STP_BRIDGE* bridge)
{
	return bridge->trees[CIST_INDEX]->rootTimes.ForwardDelay;
}

// ============================================================================

extern "C" void STP_SetTxHoldCount (struct STP_BRIDGE* bridge, unsigned int txHoldCount, unsigned int timestamp)
{
	assert (txHoldCount >= 1 && txHoldCount <= 10); // Table 13-5 in 802.1Q-2018.
	if (bridge->TxHoldCount != txHoldCount)
	{
		bridge->TxHoldCount = txHoldCount;
		for (unsigned int pi = 0; pi < bridge->portCount; pi++)
			bridge->ports[pi]->txCount = 0;
	}
}

extern "C" unsigned int STP_GetTxHoldCount (const struct STP_BRIDGE* bridge)
{
	return bridge->TxHoldCount;
}

extern "C" unsigned int STP_GetTxCount (const struct STP_BRIDGE* bridge, unsigned int portIndex)
{
	return bridge->ports[portIndex]->txCount;
}

