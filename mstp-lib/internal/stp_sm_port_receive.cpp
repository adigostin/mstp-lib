
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_procedures.h"
#include "stp_conditions_and_params.h"
#include "stp_bridge.h"
#include <assert.h>

// This file implements 13.31 from 802.1Q-2018.

using namespace PortReceive;

#if STP_USE_LOG
static const char* GetStateName (State state)
{
	switch (state)
	{
		case DISCARD:	return "DISCARD";
		case RECEIVE:	return "RECEIVE";
		default:		return "(undefined)";
	}
}
#endif

// ============================================================================

// Returns the new state, or 0 when no transition is to be made.
static State CheckConditions (const STP_BRIDGE* bridge, PortIndex givenPort, State state)
{
	PORT* port = bridge->ports[givenPort];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN
		|| ((port->rcvdBpdu || (port->edgeDelayWhile != bridge->MigrateTime)) && !port->portEnabled))
	{
		if (state == DISCARD)
		{
			// The entry block for this state has been executed already.
			return (State)0;
		}

		return DISCARD;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == DISCARD)
	{
		if (port->rcvdBpdu && port->portEnabled && port->enableBPDUrx)
			return RECEIVE;

		return (State)0;
	}

	if (state == RECEIVE)
	{
		if (port->rcvdBpdu && port->portEnabled && port->enableBPDUrx && !rcvdAnyMsg (bridge, givenPort))
			return RECEIVE;

		return (State)0;
	}

	assert (false);
	return (State)0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, PortIndex givenPort, State state, unsigned int timestamp)
{
	PORT* port = bridge->ports[givenPort];

	if (state == DISCARD)
	{
		port->rcvdBpdu = port->rcvdRSTP = port->rcvdSTP = false;
		port->agreedMisorder = true; port->agreedN = port->agreedND = port->agreeND = 0; port->agreeN = 1;
		clearAllRcvdMsgs (bridge, givenPort);
		port->edgeDelayWhile = bridge->MigrateTime;
	}
	else if (state == RECEIVE)
	{
		updtBPDUVersion (bridge, givenPort);
		port->rcvdInternal = fromSameRegion (bridge, givenPort);
		rcvMsgs (bridge, givenPort);
		port->operEdge = port->isolate = port->rcvdBpdu = false;
		port->edgeDelayWhile = bridge->MigrateTime;
	}
	else
		assert (false);
}

const StateMachine<PortReceive::State, PortIndex> PortReceive::sm =
{
#if STP_USE_LOG
	"PortReceive",
	&GetStateName,
#endif
	&CheckConditions,
	&InitState
};
