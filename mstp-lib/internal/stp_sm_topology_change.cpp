
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

// This file implements §13.39 from 802.1Q-2018.

#include "stp_procedures.h"
#include "stp_conditions_and_params.h"
#include "stp_bridge.h"
#include "stp_log.h"
#include "stp_sm.h"
#include <assert.h>

using namespace TopologyChange;

#if STP_USE_LOG
static const char* GetStateName (TopologyChange::State state)
{
	switch (state)
	{
		case ACTIVE:       return "ACTIVE";
		case INACTIVE:     return "INACTIVE";
		case LEARNING:     return "LEARNING";
		case DETECTED:     return "DETECTED";
		case NOTIFIED_TCN: return "NOTIFIED_TCN";
		case NOTIFIED_TC:  return "NOTIFIED_TC";
		case PROPAGATING:  return "PROPAGATING";
		case ACKNOWLEDGED: return "ACKNOWLEDGED";
		default:               return "(undefined)";
	}
}
#endif

// ============================================================================

// Returns the new state, or 0 when no transition is to be made.
static TopologyChange::State CheckConditions (const STP_BRIDGE* bridge, PortAndTree pt, TopologyChange::State state)
{
	PortIndex givenPort = pt.portIndex;
	TreeIndex givenTree = pt.treeIndex;

	PORT* port = bridge->ports[givenPort];
	PORT_TREE* portTree = port->trees[givenTree];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN)
	{
		if (state == INACTIVE)
		{
			// The entry block for this state has been executed already.
			return (TopologyChange::State) 0;
		}

		return INACTIVE;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == ACTIVE)
	{
		if (((portTree->role != STP_PORT_ROLE_ROOT) && (portTree->role != STP_PORT_ROLE_DESIGNATED) && (portTree->role != STP_PORT_ROLE_MASTER)) || port->operEdge)
			return LEARNING;

		if (port->rcvdTcn)
			return NOTIFIED_TCN;

		if (portTree->rcvdTc)
			return NOTIFIED_TC;

		if (portTree->tcProp && !port->operEdge)
			return PROPAGATING;

		if (port->rcvdTcAck)
			return ACKNOWLEDGED;

		return (TopologyChange::State) 0;
	}

	if (state == INACTIVE)
	{
		if (portTree->learn && !portTree->fdbFlush)
			return LEARNING;

		return (TopologyChange::State) 0;
	}

	if (state == LEARNING)
	{
		if (((portTree->role == STP_PORT_ROLE_ROOT) || (portTree->role == STP_PORT_ROLE_DESIGNATED) || (portTree->role == STP_PORT_ROLE_MASTER)) && portTree->forward && !port->operEdge)
			return DETECTED;

		if ((portTree->role != STP_PORT_ROLE_ROOT) && (portTree->role != STP_PORT_ROLE_DESIGNATED) && (portTree->role != STP_PORT_ROLE_MASTER) && !(portTree->learn || portTree->learning) && !(portTree->rcvdTc || port->rcvdTcn || port->rcvdTcAck || portTree->tcProp))
			return INACTIVE;

		if (portTree->rcvdTc || port->rcvdTcn || port->rcvdTcAck || portTree->tcProp)
			return LEARNING;

		return (TopologyChange::State) 0;
	}

	if (state == DETECTED)
		return ACTIVE;

	if (state == NOTIFIED_TCN)
		return NOTIFIED_TC;

	if (state == NOTIFIED_TC)
		return ACTIVE;

	if (state == PROPAGATING)
		return ACTIVE;

	if (state == ACKNOWLEDGED)
		return ACTIVE;

	assert (false);
	return (TopologyChange::State) 0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, PortAndTree pt, TopologyChange::State state, unsigned int timestamp)
{
	PortIndex givenPort = pt.portIndex;
	TreeIndex givenTree = pt.treeIndex;

	PORT* port = bridge->ports[givenPort];
	PORT_TREE* portTree = port->trees[givenTree];

	if (state == ACTIVE)
	{
	}
	else if (state == INACTIVE)
	{
		//portTree->fdbFlush = true;
		// We don't set this variable. Instead, we call the flush callback directly from here, require the callback to wait for completion,
		// and we keep the variable always clear. This keeps the code simple, but will create problems in case some switch IC takes
		// a long time to clear its FDB entries.
		//
		// Maybe we should change the library as follows:
		// Call here a callback that only _initiates_ FDB flushing and returns, and require the application to call a new STP function upon completion.
		// This wouldn't be a simple matter, because these asynchronous flush operations might overlap.
		//
		// 13.25.13 fdbFlush
		// A Boolean. Set by the topology change state machine to instruct the filtering database to remove entries for
		// this port, immediately if rstpVersion (13.26.19) is TRUE, or by rapid ageing (13.25.2) if stpVersion
		// (13.26.20) is TRUE. Reset by the filtering database once the entries are removed if rstpVersion is TRUE, and
		// immediately if stpVersion is TRUE. Setting the fdbFlush variable does not result in removal of filtering
		// database entries in the case that the port is an Edge Port (i.e., operEdge is TRUE). The filtering database
		// removes entries only for those VIDs that have a fixed registration (see 10.7.2) on any port of the bridge that
		// is not an Edge Port.
		if (port->operEdge == false)
		{
			FLUSH_LOG (bridge);

			bridge->callbacks.flushFdb (bridge, givenPort, givenTree, rstpVersion (bridge) ? STP_FLUSH_FDB_TYPE_IMMEDIATE : STP_FLUSH_FDB_TYPE_RAPID_AGEING);
		}

		portTree->tcDetected = 0;
		portTree->tcWhile = 0;
		if (givenTree == CIST_INDEX)
			port->tcAck = false;
	}
	else if (state == LEARNING)
	{
		if (givenTree == CIST_INDEX)
			portTree->rcvdTc = port->rcvdTcn = port->rcvdTcAck = false;

		portTree->rcvdTc = portTree->tcProp = false;
	}
	else if (state == DETECTED)
	{
		newTcWhile (bridge, givenPort, givenTree, timestamp);
		setTcPropTree (bridge, givenPort, givenTree);
		newTcDetected (bridge, givenPort, givenTree);
		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}
	else if (state == NOTIFIED_TCN)
	{
		newTcWhile (bridge, givenPort, givenTree, timestamp);
	}
	else if (state == NOTIFIED_TC)
	{
		// Note AG: Added by me, see comment at the top of the function.
		CallNotifiedTcCallback (bridge, givenTree, timestamp);

		if (givenTree == CIST_INDEX)
			port->rcvdTcn = false;
		portTree->rcvdTc = false;
		if ((givenTree == CIST_INDEX) && (portTree->role == STP_PORT_ROLE_DESIGNATED))
			port->tcAck = true;
		setTcPropTree (bridge, givenPort, givenTree);
	}
	else if (state == PROPAGATING)
	{
		newTcWhile (bridge, givenPort, givenTree, timestamp);

		//portTree->fdbFlush = true;
		// See comments for the INACTIVE state above in this function.
		if (port->operEdge == false)
		{
			FLUSH_LOG (bridge);

			bridge->callbacks.flushFdb (bridge, givenPort, givenTree, rstpVersion (bridge) ? STP_FLUSH_FDB_TYPE_IMMEDIATE : STP_FLUSH_FDB_TYPE_RAPID_AGEING);
		}

		portTree->tcProp = false;
	}
	else if (state == ACKNOWLEDGED)
	{
		portTree->tcWhile = 0;
		port->rcvdTcAck = false;
	}
	else
		assert (false);
}

const StateMachine<TopologyChange::State, PortAndTree> TopologyChange::sm =
{
#if STP_USE_LOG
	"TopologyChange",
	&GetStateName,
#endif
	&CheckConditions,
	&InitState
};
