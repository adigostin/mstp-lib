
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>

// See §13.29 in 802.1Q-2011

enum
{
	UNDEFINED,
	DISCARD,
	RECEIVE,
};

// ============================================================================

const char*	PortReceive_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case DISCARD:	return "DISCARD";
		case RECEIVE:	return "RECEIVE";
		default:		return "(undefined)";
	}
}

// ============================================================================

SM_STATE PortReceive_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN
		|| ((port->rcvdBpdu || (port->edgeDelayWhile != bridge->MigrateTime)) && !port->portEnabled))
	{
		if (state == DISCARD)
		{
			// The entry block for this state has been executed already.
			return 0;
		}

		return DISCARD;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == DISCARD)
	{
		if (port->rcvdBpdu && port->portEnabled && port->enableBPDUrx)
			return RECEIVE;

		return 0;
	}

	if (state == RECEIVE)
	{
		if (port->rcvdBpdu && port->portEnabled && port->enableBPDUrx && !rcvdAnyMsg (bridge, givenPort))
			return RECEIVE;

		return 0;
	}

	assert (false);
	return 0;
}

// ============================================================================

void PortReceive_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];

	if (state == DISCARD)
	{
		port->rcvdBpdu = port->rcvdRSTP = port->rcvdSTP = false;
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
