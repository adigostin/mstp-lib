
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include <assert.h>

// See §13.32 in 802.1Q-2011

using namespace PortTransmit;

static const char* GetStateName (State state)
{
	switch (state)
	{
		case TRANSMIT_INIT:     return "TRANSMIT_INIT";
		case TRANSMIT_PERIODIC: return "TRANSMIT_PERIODIC";
		case TRANSMIT_CONFIG:   return "TRANSMIT_CONFIG";
		case TRANSMIT_TCN:      return "TRANSMIT_TCN";
		case TRANSMIT_RSTP:     return "TRANSMIT_RSTP";
		case AGREE_SPT:         return "AGREE_SPT";
		case IDLE:              return "IDLE";
		default:                return "(undefined)";
	}
}

// ============================================================================

// Returns the new state, or 0 when no transition is to be made.
static State CheckConditions (const STP_BRIDGE* bridge, PortIndex givenPort, State state)
{
	PORT* port = bridge->ports[givenPort];

	// ------------------------------------------------------------------------
	// Check global conditions.
	
	if (bridge->BEGIN || !port->portEnabled || !port->enableBPDUtx)
	{
		if (state == TRANSMIT_INIT)
		{
			// The entry block for this state has been executed already.
			return (State)0;
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
	
	if (state == AGREE_SPT)
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
			
			if (port->sendRSTP && (port->newInfo || (port->newInfoMsti && !mstiMasterPort (bridge, givenPort))) && (port->txCount < bridge->TxHoldCount) && (port->helloWhen != 0))
				return TRANSMIT_RSTP;

			if (spt(bridge) && port->sendRSTP && allSptAgree(bridge) && !port->agreeDigestValid)
				return AGREE_SPT;
		}
		
		return (State)0;
	}
	
	assert (false);
	return (State)0;
}

// ============================================================================

static void InitState (STP_BRIDGE* bridge, PortIndex givenPort, State state, unsigned int timestamp)
{
	PORT* port = bridge->ports[givenPort];

	if (state == TRANSMIT_INIT)
	{
		port->newInfo = port->newInfoMsti = true;
		port->txCount = 0;
	}
	else if (state == TRANSMIT_PERIODIC)
	{
		// Note AG: Not clear in the standard: tcWhile of which tree? I'll assume they meant "CIST's tcWhile", since the whole expression is about the CIST.
		port->newInfo = port->newInfo || (cistDesignatedPort (bridge, givenPort) || (cistRootPort (bridge, givenPort) && (port->trees[CIST_INDEX]->tcWhile != 0)));

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
	else if (state == AGREE_SPT)
	{
		port->agreeDigestValid = true; port->newInfoMsti = true;
	}
	else if (state == IDLE)
	{
		port->helloWhen = HelloTime (bridge, givenPort);
	}
	else
		assert (false);
}

const PerPortStateMachine<PortTransmit::State> PortTransmit::sm =
{
	"PortTransmit",
	&GetStateName,
	&CheckConditions,
	&InitState
};
