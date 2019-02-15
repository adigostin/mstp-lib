
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include <assert.h>

// See §13.38 in 802.1Q-2011

using namespace L2GPortReceive;

static const char* GetStateName (State state)
{
	switch (state)
	{
		case UNDEFINED:			return "UNDEFINED";
		case INIT:				return "INIT";
		case PSEUDO_RECEIVE:	return "PSEUDO_RECEIVE";
		case DISCARD:			return "DISCARD";
		case L2GP:				return "L2GP";
		default: assert (false); return nullptr;
	}
}

// ============================================================================

// Returns the new state, or 0 when no transition is to be made.
static State CheckConditions (const STP_BRIDGE* bridge, PortIndex givenPort, State state)
{
	PORT* port = bridge->ports [givenPort];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN || !port->isL2gp || !port->portEnabled)
	{
		if (state == INIT)
		{
			// The entry block for this state has been executed already.
			return (State)0;
		}

		return INIT;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == INIT)
		return L2GP;

	if (state == PSEUDO_RECEIVE)
		return L2GP;

	if (state == DISCARD)
		return L2GP;

	if (state == L2GP)
	{
		if (!port->enableBPDUrx && port->rcvdBpdu && !rcvdAnyMsg (bridge, givenPort))
			return DISCARD;

		if ((port->pseudoInfoHelloWhen == 0) && !rcvdAnyMsg (bridge, givenPort))
			return PSEUDO_RECEIVE;

		return (State)0;
	}

	assert (false);
	return (State)0;
}

static void InitState (STP_BRIDGE* bridge, PortIndex givenPort, State state, unsigned int timestamp)
{
	PORT* port = bridge->ports[givenPort];

	if (state == INIT)
	{
		port->pseudoInfoHelloWhen = 0;
	}
	else if (state == PSEUDO_RECEIVE)
	{
		port->rcvdInternal = true;
		pseudoRcvMsgs (bridge, givenPort);
		port->edgeDelayWhile = bridge->MigrateTime;
		port->pseudoInfoHelloWhen = HelloTime (bridge, givenPort);
	}
	else if (state == DISCARD)
	{
		port->rcvdBpdu = false;
	}
	else if (state == L2GP)
	{
	}
	else
		assert (false);
}

const PerPortStateMachine<State> L2GPortReceive::sm = 
{
	"L2GPortReceive",
	&GetStateName,
	&CheckConditions,
	&InitState
};

