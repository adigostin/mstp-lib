
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

// This file implements 13.28 from 802.1Q-2018.

#include "stp_conditions_and_params.h"
#include "stp_bridge.h"
#include <assert.h>

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
//       port are both within the Bridge's SPT Region, and both learning and forwarding are FALSE for
//       the given port; or
//    4) Master Port and synced is TRUE for all ports for the given tree other than the given port.
bool allSynced (const STP_BRIDGE* bridge, PortIndex givenPort, TreeIndex givenTree)
{
	// a) For all ports for the given tree, selected is TRUE, the port's role is the same as its selectedRole, and updtInfo is FALSE; and
	for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
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
		for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
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
		for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
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
// NOTE-The dependency of rcvdMstiMsg and updtMstiInfo on CIST variables for the port reflects the fact that MSTIs
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
