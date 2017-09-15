
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>

// See §13.30 in 802.1Q-2011

enum
{
	UNDEFINED,
	CHECKING_RSTP,
	SELECTING_STP,
	SENSING,
};

// ============================================================================

const char* PortProtocolMigration_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case CHECKING_RSTP:		return "CHECKING_RSTP";
		case SELECTING_STP:		return "SELECTING_STP";
		case SENSING:			return "SENSING";
		default:				return "(undefined)";
	}
}

// ============================================================================

SM_STATE PortProtocolMigration_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];
	
	// ------------------------------------------------------------------------
	// Check global conditions.
	
	if (bridge->BEGIN)
	{
		if (state == CHECKING_RSTP)
		{
			// The entry block for this state has been executed already.
			return 0;
		}
		
		return CHECKING_RSTP;
	}
	
	// ------------------------------------------------------------------------
	// Check exit conditions from each state.
	
	if (state == CHECKING_RSTP)
	{
		// Problem in the specs: These two exit conditions could both be true at the same time:
		// 1. (mdelayWhile != MigrateTime) && !portEnabled
		// 2. (mDelayWhile == 0)
		// If we check condition 1 first, this would result in mDelayWhile being set to non-zero,
		// so we'll never have the chance to check condition 2, so the state machine would never exit the CHECKING_RSTP state.
		// This doesn't make sense, so let's first check condition 2.

		if (port->mDelayWhile == 0)
			return SENSING;
		
		if ((port->mDelayWhile != bridge->MigrateTime) && !port->portEnabled)
			return CHECKING_RSTP;
		
		return 0;
	}
	
	if (state == SELECTING_STP)
	{
		if ((port->mDelayWhile == 0) || !port->portEnabled || port->mcheck)
			return SENSING;
		
		return 0;
	}

	if (state == SENSING)
	{
		if (port->sendRSTP && port->rcvdSTP)
			return SELECTING_STP;
		
		if (!port->portEnabled || port->mcheck || ((rstpVersion (bridge) && !port->sendRSTP && port->rcvdRSTP)))
			return CHECKING_RSTP;
		
		return 0;
	}

	assert (false);
	return 0;
}

// ============================================================================

void PortProtocolMigration_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];
	
	if (state == CHECKING_RSTP)
	{
		port->mcheck = false;
		port->sendRSTP = rstpVersion (bridge);
		port->mDelayWhile = bridge->MigrateTime;
	}
	else if (state == SELECTING_STP)
	{
		port->sendRSTP = false;
		port->mDelayWhile = bridge->MigrateTime;
	}		
	else if (state == SENSING)
	{
		port->rcvdRSTP = port->rcvdSTP = false;
	}
	else
		assert (false);
}
