
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>

// See §13.34 in 802.1Q-2011

enum
{
	UNDEFINED,
	INIT_TREE,
	ROLE_SELECTION,
};

// ============================================================================

const char* PortRoleSelection_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case INIT_TREE:		return "INIT_TREE";
		case ROLE_SELECTION:return "ROLE_SELECTION";
		default:			return "(undefined)";
	}
}

// ============================================================================

SM_STATE PortRoleSelection_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenTree != -1);
	assert (givenPort == -1);

	// ------------------------------------------------------------------------
	// Check global conditions.
	
	if (bridge->BEGIN)
	{
		if (state == INIT_TREE)
		{
			// The entry block for this state has been executed already.
			return 0;
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
		
		return 0;
	}

	assert (false);
	return 0;
}

// ============================================================================

void PortRoleSelection_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenTree != -1);
	assert (givenPort == -1);

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
