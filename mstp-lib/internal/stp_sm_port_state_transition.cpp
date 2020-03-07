
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

// This file implements 13.38 from 802.1Q-2018.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include <assert.h>

using namespace PortStateTransition;

#if STP_USE_LOG
static const char* GetStateName (State state)
{
	switch (state)
	{
		case DISCARDING:	return "DISCARDING";
		case LEARNING:		return "LEARNING";
		case FORWARDING:	return "FORWARDING";
		default:			return "(undefined)";
	}
}
#endif

// ============================================================================

// Returns the new state, or 0 when no transition is to be made.
static State CheckConditions (const STP_BRIDGE* bridge, PortAndTree pt, State state)
{
	PortIndex givenPort = pt.portIndex;
	TreeIndex givenTree = pt.treeIndex;

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN)
	{
		if (state == DISCARDING)
		{
			// The entry block for this state has been executed already.
			return (State)0;
		}

		return DISCARDING;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == DISCARDING)
	{
		if (tree->learn)
			return LEARNING;

		return (State)0;
	}

	if (state == LEARNING)
	{
		if (!tree->learn)
			return DISCARDING;

		if (tree->forward)
			return FORWARDING;

		return (State)0;
	}

	if (state == FORWARDING)
	{
		if (!tree->forward)
			return DISCARDING;

		return (State)0;
	}

	assert (false);
	return (State)0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, PortAndTree pt, State state, unsigned int timestamp)
{
	PortIndex givenPort = pt.portIndex;
	TreeIndex givenTree = pt.treeIndex;

	PORT* port = bridge->ports[givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	if (state == DISCARDING)
	{
		disableLearning (bridge, givenPort, givenTree, timestamp);
		tree->learning = false;
		disableForwarding (bridge, givenPort, givenTree, timestamp);
		tree->forwarding = false;
	}
	else if (state == LEARNING)
	{
		enableLearning (bridge, givenPort, givenTree, timestamp);
		tree->learning = true;
	}
	else if (state == FORWARDING)
	{
		enableForwarding (bridge, givenPort, givenTree, timestamp);
		tree->forwarding = true;
	}
	else
		assert (false);
}

const StateMachine<PortStateTransition::State, PortAndTree> PortStateTransition::sm =
{
#if STP_USE_LOG
	"PortStateTransition",
	&GetStateName,
#endif
	&CheckConditions,
	&InitState
};
