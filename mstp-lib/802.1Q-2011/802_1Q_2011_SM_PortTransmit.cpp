
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>

// See §13.32 in 802.1Q-2011

enum
{
	UNDEFINED,
	TRANSMIT_INIT,
	TRANSMIT_PERIODIC,
	TRANSMIT_CONFIG,
	TRANSMIT_TCN,
	TRANSMIT_RSTP,
	IDLE,
};

// ============================================================================

const char* PortTransmit_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case TRANSMIT_INIT:		return "TRANSMIT_INIT";
		case TRANSMIT_PERIODIC:	return "TRANSMIT_PERIODIC";
		case TRANSMIT_CONFIG:	return "TRANSMIT_CONFIG";
		case TRANSMIT_TCN:		return "TRANSMIT_TCN";
		case TRANSMIT_RSTP:		return "TRANSMIT_RSTP";
		case IDLE:				return "IDLE";
		default:				return "(undefined)";
	}
}

// ============================================================================
// Returns the new state, or 0 when no transition was made.
SM_STATE PortTransmit_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree == -1);
	
	PORT* port = bridge->ports [givenPort];

	// ------------------------------------------------------------------------
	// Check global conditions.
	
	if (bridge->BEGIN || !port->portEnabled || !port->enableBPDUtx)
	{
		if (state == TRANSMIT_INIT)
		{
			// The entry block for this state has been executed already.
			return 0;
		}
		
		return TRANSMIT_INIT;
	}
	
	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	if (state == TRANSMIT_INIT)
		return IDLE;

	if (state == TRANSMIT_PERIODIC)
		return IDLE;

	if (state == TRANSMIT_CONFIG)
		return IDLE;
	
	if (state == TRANSMIT_TCN)
		return IDLE;
	
	if (state == TRANSMIT_RSTP)
		return IDLE;
	
	if (state == IDLE)
	{
		if (allTransmitReady (bridge, givenPort))
		{
			if (port->helloWhen == 0)
				return TRANSMIT_PERIODIC;
			
			if (!port->sendRSTP && port->newInfo && cistDesignatedPort (bridge, givenPort) && (port->txCount < bridge->TxHoldCount) && (port->helloWhen != 0))
				return TRANSMIT_CONFIG;
	
			if (!port->sendRSTP && port->newInfo && cistRootPort (bridge, givenPort) && (port->txCount < bridge->TxHoldCount) && (port->helloWhen != 0))
				return TRANSMIT_TCN;
			
			if (port->sendRSTP && (port->newInfo || (port->newInfoMsti && !mstiMasterPort (bridge, givenPort))) && (port->txCount < bridge->TxHoldCount) && (port->helloWhen !=0))
				return TRANSMIT_RSTP;
		}
		
		return 0; // no change of state;
	}
	
	assert (false);
	return 0;
}

// ============================================================================

void PortTransmit_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree == -1);
	
	PORT* port = bridge->ports [givenPort];

	if (state == TRANSMIT_INIT)
	{
		port->newInfo = port->newInfoMsti = true;
		port->txCount = 0;
	}
	else if (state == TRANSMIT_PERIODIC)
	{
		port->newInfo = port->newInfo || (cistDesignatedPort (bridge, givenPort) || (cistRootPort (bridge, givenPort) && (port->trees [CIST_INDEX]->tcWhile != 0)));
		port->newInfoMsti = port->newInfoMsti || mstiDesignatedOrTCpropagatingRootPort (bridge, givenPort);
	}
	else if (state == TRANSMIT_CONFIG)
	{
		port->newInfo = false;
		txConfig (bridge, givenPort, timestamp);
		port->txCount += 1;
		port->tcAck = false;
	}
	else if (state == TRANSMIT_TCN)
	{
		port->newInfo = false;
		txTcn (bridge, givenPort, timestamp);
		port->txCount += 1;
	}		
	else if (state == TRANSMIT_RSTP)
	{
		port->newInfo = port->newInfoMsti = false;
		txRstp (bridge, givenPort, timestamp);
		port->txCount += 1;
		port->tcAck = false;
	}		
	else if (state == IDLE)
	{
		port->helloWhen = HelloTime (bridge, givenPort);
	}
	else
		assert (false);
}


