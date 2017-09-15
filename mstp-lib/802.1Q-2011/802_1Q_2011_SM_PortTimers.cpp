
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>

// See §13.28 in 802.1Q-2011

enum
{
	UNDEFINED,
	ONE_SECOND,
	TICK,
};

// ============================================================================

const char*	PortTimers_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case ONE_SECOND:	return "ONE_SECOND";
		case TICK:			return "TICK";
		default:			return "(undefined)";
	}
}

// ============================================================================

SM_STATE PortTimers_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];
	
	// ------------------------------------------------------------------------
	// Check global conditions.
	
	if (bridge->BEGIN)
	{
		if (state == ONE_SECOND)
		{
			// The entry block for this state has been executed already.
			return 0;
		}
		
		return ONE_SECOND;
	}
	
	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == ONE_SECOND)
	{
		if (port->tick)
			return TICK;
		
		return 0;
	}

	if (state == TICK)
		return ONE_SECOND;
	
	assert (false);
	return 0;
}

// ============================================================================

void PortTimers_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];
	
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
	else
		assert (false);
}


// ============================================================================
