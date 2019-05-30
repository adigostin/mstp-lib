
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

// This file implements 13.32 from 802.1Q-2018

#include "stp_procedures.h"
#include "stp_conditions_and_params.h"
#include "stp_bridge.h"
#include <assert.h>

using namespace PortProtocolMigration;

#if STP_USE_LOG
static const char* GetStateName (State state)
{
	switch (state)
	{
		case CHECKING_RSTP:		return "CHECKING_RSTP";
		case SELECTING_STP:		return "SELECTING_STP";
		case SENSING:			return "SENSING";
		default:				return "(undefined)";
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
		if (state == CHECKING_RSTP)
		{
			// The entry block for this state has been executed already.
			return (State)0;
		}

		return CHECKING_RSTP;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == CHECKING_RSTP)
	{
		if (port->mDelayWhile == 0)
			return SENSING;

		if ((port->mDelayWhile != bridge->MigrateTime) && !port->portEnabled)
			return CHECKING_RSTP;

		return (State)0;
	}

	if (state == SELECTING_STP)
	{
		if ((port->mDelayWhile == 0) || !port->portEnabled || port->mcheck)
			return SENSING;

		return (State)0;
	}

	if (state == SENSING)
	{
		if (port->sendRSTP && port->rcvdSTP)
			return SELECTING_STP;

		if (!port->portEnabled || port->mcheck || ((rstpVersion(bridge) && !port->sendRSTP && port->rcvdRSTP)))
			return CHECKING_RSTP;

		return (State)0;
	}

	assert (false);
	return (State)0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, PortIndex givenPort, State state, unsigned int timestamp)
{
	PORT* port = bridge->ports[givenPort];

	if (state == CHECKING_RSTP)
	{
		port->mcheck = false;
		port->sendRSTP = rstpVersion(bridge);
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

const StateMachine<PortProtocolMigration::State, PortIndex> PortProtocolMigration::sm =
{
#if STP_USE_LOG
	"PortProtocolMigration",
	&GetStateName,
#endif
	&CheckConditions,
	&InitState
};

