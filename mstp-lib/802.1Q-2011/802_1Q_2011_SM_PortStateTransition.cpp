
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>

// See §13.36 in 802.1Q-2011

enum
{
	UNDEFINED,
	DISCARDING,
	LEARNING,
	FORWARDING,
};

// ============================================================================

const char* PortStateTransition_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case DISCARDING:	return "DISCARDING";
		case LEARNING:		return "LEARNING";
		case FORWARDING:	return "FORWARDING";
		default:			return "(undefined)";
	}
}

// ============================================================================

SM_STATE PortStateTransition_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree != -1);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN)
	{
		if (state == DISCARDING)
		{
			// The entry block for this state has been executed already.
			return 0;
		}

		return DISCARDING;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == DISCARDING)
	{
		if (tree->learn)
			return LEARNING;

		return 0;
	}

	if (state == LEARNING)
	{
		if (!tree->learn)
			return DISCARDING;

		if (tree->forward)
			return FORWARDING;

		return 0;
	}

	if (state == FORWARDING)
	{
		if (!tree->forward)
			return DISCARDING;

		return 0;
	}

	assert (false);
	return 0;
}

// ============================================================================

void PortStateTransition_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree != -1);

	PORT* port = bridge->ports [givenPort];
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
