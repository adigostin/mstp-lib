
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include "../stp_log.h"
#include <assert.h>

// See 13.37 in 802.1Q-2011

enum
{
	UNDEFINED,
	ACTIVE,
	INACTIVE,
	LEARNING,
	DETECTED,
	NOTIFIED_TCN,
	NOTIFIED_TC,
	PROPAGATING,
	ACKNOWLEDGED,
};

// ============================================================================

const char* TopologyChange_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case ACTIVE:		return "ACTIVE";
		case INACTIVE:		return "INACTIVE";
		case LEARNING:		return "LEARNING";
		case DETECTED:		return "DETECTED";
		case NOTIFIED_TCN:	return "NOTIFIED_TCN";
		case NOTIFIED_TC:	return "NOTIFIED_TC";
		case PROPAGATING:	return "PROPAGATING";
		case ACKNOWLEDGED:	return "ACKNOWLEDGED";
		default:			return "(undefined)";
	}
}

// ============================================================================

// When this function returns a valid state (non-zero), it means it has changed one or more variables, so all state machines must be run again.
SM_STATE TopologyChange_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree != -1);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN)
	{
		if (state == INACTIVE)
		{
			// The entry block for this state has been executed already.
			return 0;
		}

		return INACTIVE;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == ACTIVE)
	{
		if (((portTree->role != STP_PORT_ROLE_ROOT) && (portTree->role != STP_PORT_ROLE_DESIGNATED) && (portTree->role != STP_PORT_ROLE_MASTER)) || port->operEdge)
		{
			// Note AG: Added by me, see comment at the top of the function.
			// Added here to avoid introducing a new state between ACTIVE and LEARNING,
			// because a new state might confuse the application programmer.
			CallTcCallback (bridge);

			return LEARNING;
		}

		if (port->rcvdTcn)
			return NOTIFIED_TCN;

		if (portTree->rcvdTc)
			return NOTIFIED_TC;

		if (portTree->tcProp && !port->operEdge)
			return PROPAGATING;

		if (port->rcvdTcAck)
			return ACKNOWLEDGED;

		return 0;
	}

	if (state == INACTIVE)
	{
		if (portTree->learn && !portTree->fdbFlush)
			return LEARNING;

		return 0;
	}

	if (state == LEARNING)
	{
		if (((portTree->role == STP_PORT_ROLE_ROOT) || (portTree->role == STP_PORT_ROLE_DESIGNATED) || (portTree->role == STP_PORT_ROLE_MASTER)) && portTree->forward && !port->operEdge)
			return DETECTED;

		if ((portTree->role != STP_PORT_ROLE_ROOT) && (portTree->role != STP_PORT_ROLE_DESIGNATED) && (portTree->role != STP_PORT_ROLE_MASTER) && !(portTree->learn || portTree->learning) && !(portTree->rcvdTc || port->rcvdTcn || port->rcvdTcAck || portTree->tcProp))
			return INACTIVE;

		if (portTree->rcvdTc || port->rcvdTcn || port->rcvdTcAck || portTree->tcProp)
			return LEARNING;

		return 0;
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
	return 0;
}

// ============================================================================

void TopologyChange_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree != -1);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* portTree = port->trees [givenTree];

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
		// Note AG: Added by me, see comment at the top of the function.
		CallTcCallback (bridge);

		newTcWhile (bridge, givenPort, givenTree);
		setTcPropTree (bridge, givenPort, givenTree);
		newTcDetected (bridge, givenPort, givenTree);
		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}
	else if (state == NOTIFIED_TCN)
	{
		newTcWhile (bridge, givenPort, givenTree);
	}
	else if (state == NOTIFIED_TC)
	{
		// Note AG: Added by me, see comment at the top of the function.
		CallNotifiedTcCallback (bridge, (unsigned int) givenTree, timestamp);

		if (givenTree == CIST_INDEX)
			port->rcvdTcn = false;
		portTree->rcvdTc = false;
		if ((givenTree == CIST_INDEX) && (portTree->role == STP_PORT_ROLE_DESIGNATED))
			port->tcAck = true;
		setTcPropTree (bridge, givenPort, givenTree);
	}
	else if (state == PROPAGATING)
	{
		newTcWhile (bridge, givenPort, givenTree);

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
