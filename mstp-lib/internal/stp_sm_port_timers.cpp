
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include <assert.h>

// This file implements 13.30 from 802.1Q-2018.

using namespace PortTimers;

#if STP_USE_LOG
static const char* GetStateName (State state)
{
	switch (state)
	{
		case ONE_SECOND:	return "ONE_SECOND";
		case TICK:			return "TICK";
		default:			return "(undefined)";
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

	if (bridge->BEGIN)
	{
		if (state == ONE_SECOND)
		{
			// The entry block for this state has been executed already.
			return (State)0;
		}

		return ONE_SECOND;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == ONE_SECOND)
	{
		if (port->tick)
			return TICK;

		return (State)0;
	}

	if (state == TICK)
		return ONE_SECOND;

	assert (false);
	return (State)0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, PortIndex givenPort, State state, unsigned int timestamp)
{
	PORT* port = bridge->ports[givenPort];

	if (state == ONE_SECOND)
	{
		port->tick = false;
	}
	else if (state == TICK)
	{
		if (port->helloWhen      > 0) port->helloWhen--;
		if (port->mDelayWhile    > 0) port->mDelayWhile--;
		if (port->edgeDelayWhile > 0) port->edgeDelayWhile--;
		if (port->txCount        > 0) port->txCount--;
		if (port->pseudoInfoHelloWhen > 0) port->pseudoInfoHelloWhen--;

		for (unsigned int treeIndex = 0; treeIndex < bridge->treeCount(); treeIndex++)
		{
			PORT_TREE* portTree = port->trees [treeIndex];

			if (portTree->tcWhile       > 0) portTree->tcWhile--;
			if (portTree->fdWhile       > 0) portTree->fdWhile--;
			if (portTree->rcvdInfoWhile > 0) portTree->rcvdInfoWhile--;
			if (portTree->rrWhile       > 0) portTree->rrWhile--;
			if (portTree->tcDetected    > 0) portTree->tcDetected--;
			if (portTree->rbWhile       > 0) portTree->rbWhile--;
		}
	}
}

const StateMachine<State, PortIndex> PortTimers::sm =
{
#if STP_USE_LOG
	"PortTimers",
	&GetStateName,
#endif
	&CheckConditions,
	&InitState
};
