
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include <assert.h>

// This file implements 13.36 from 802.1Q-2018.

using namespace PortRoleSelection;

#if STP_USE_LOG
static const char* GetStateName (State state)
{
	switch (state)
	{
		case INIT_TREE:		return "INIT_TREE";
		case ROLE_SELECTION:return "ROLE_SELECTION";
		default:			return "(undefined)";
	}
}
#endif

// ============================================================================

// Returns the new state, or 0 when no transition is to be made.
static State CheckConditions (const STP_BRIDGE* bridge, TreeIndex givenTree, State state)
{
	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN)
	{
		if (state == INIT_TREE)
		{
			// The entry block for this state has been executed already.
			return (State)0;
		}

		return INIT_TREE;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == INIT_TREE)
		return ROLE_SELECTION;

	if (state == ROLE_SELECTION)
	{
		for (unsigned int portIndex = 0; portIndex < bridge->portCount; portIndex++)
		{
			if (bridge->ports [portIndex]->trees [givenTree]->reselect)
				return ROLE_SELECTION;
		}

		return (State)0;
	}

	assert (false);
	return (State)0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, TreeIndex givenTree, State state, unsigned int timestamp)
{
	if (state == INIT_TREE)
	{
		updtRolesDisabledTree (bridge, givenTree);
	}
	else if (state == ROLE_SELECTION)
	{
		clearReselectTree (bridge, givenTree);
		updtRolesTree (bridge, givenTree);
		setSelectedTree (bridge, givenTree);
	}
	else
		assert (false);
}

const StateMachine<State, TreeIndex> PortRoleSelection::sm =
{
#if STP_USE_LOG
	"PortRoleSelection",
	&GetStateName,
#endif
	&CheckConditions,
	&InitState
};

