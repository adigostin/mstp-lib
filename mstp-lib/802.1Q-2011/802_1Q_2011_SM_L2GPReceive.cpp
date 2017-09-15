
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>
#include <stddef.h>

// See §13.38 in 802.1Q-2011

enum
{
	UNDEFINED,
	INIT,
	PSEUDO_RECEIVE,
	DISCARD,
	L2GP,
};

// ============================================================================

const char* L2GP_Receive_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case UNDEFINED:			return "UNDEFINED";
		case INIT:				return "INIT";
		case PSEUDO_RECEIVE:	return "PSEUDO_RECEIVE";
		case DISCARD:			return "DISCARD";
		case L2GP:				return "L2GP";
		default: assert (false); return NULL;
	}
}

// ============================================================================

SM_STATE L2GP_Receive_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN || !port->isL2gp || !port->portEnabled)
	{
		if (state == INIT)
		{
			// The entry block for this state has been executed already.
			return 0;
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

		return 0;
	}

	assert (false);
	return 0;
}

void L2GP_Receive_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];

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
