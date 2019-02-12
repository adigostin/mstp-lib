
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

// This file implements §13.33 from 802.1Q-2018.

#include "stp_procedures.h"
#include "stp_bridge.h"
#include <assert.h>

struct BridgeDetectionImpl : BridgeDetection
{
	static const char* GetStateName (State state)
	{
		switch (state)
		{
			case NOT_EDGE:	return "NOT_EDGE";
			case EDGE:		return "EDGE";
			case ISOLATED:	return "ISOLATED";
			default:		return "(undefined)";
		}
	}

	// ============================================================================

	// Returns the new state, or 0 when no transition is to be made.
	static State CheckConditions (const STP_BRIDGE* bridge, int givenPort, int givenTree, State state)
	{
		assert (givenPort != -1);
		assert (givenTree == -1);

		PORT* port = bridge->ports [givenPort];
	
		// ------------------------------------------------------------------------
		// Check global conditions.
	
		if (bridge->BEGIN && !port->AdminEdge)
		{
			if (state == NOT_EDGE)
			{
				// The entry block for this state has been executed already.
				return (State)0;
			}
		
			return NOT_EDGE;
		}

		if (bridge->BEGIN && port->AdminEdge)
		{
			if (state == EDGE)
			{
				// The entry block for this state has been executed already.
				return (State)0;
			}
		
			return EDGE;
		}
	
		// ------------------------------------------------------------------------
		// Check exit conditions from each state.
	
		if (state == NOT_EDGE)
		{
			// I changed this condition slightly because it was looping endlessly between EDGE and NOT_EDGE when disconnecting
			// from the root bridge a port that was connected to a non-stp device and already forwarding and whose AutoEdge was true.
			// The condition specified in 802.1Q-2018 was:
			//
			//	if ((!port->portEnabled && port->AdminEdge) ||
			//		((port->edgeDelayWhile == 0) && port->AutoEdge && port->sendRSTP && port->trees [CIST_INDEX]->proposing))
			//
			if ((!port->portEnabled && port->AdminEdge) ||
				(port->portEnabled && (port->edgeDelayWhile == 0) && port->AutoEdge && port->sendRSTP && port->trees [CIST_INDEX]->proposing))
			{
				return EDGE;
			}

			if ((port->edgeDelayWhile == 0) && !port->AdminEdge && !port->AutoEdge && port->sendRSTP && port->trees [CIST_INDEX]->proposing && port->operPointToPointMAC)
			{
				return ISOLATED;
			}

			return (State)0;
		}

		if (state == EDGE)
		{
			// I changed this condition slightly because it was looping endlessly between EDGE and NOT_EDGE when disconnecting
			// from the root bridge a port that was connected to a non-stp device and already forwarding and whose AutoEdge was true.
			// The condition specified in 802.1Q-2018 was:
			//
			//if (((!port->portEnabled || !port->AutoEdge) && !port->AdminEdge) || !port->operEdge)
			//
			if (((!port->portEnabled || !port->AutoEdge) && !port->AdminEdge) || (port->portEnabled && !port->operEdge))
			{
				return NOT_EDGE;
			}

			return (State)0;
		}
	
		if (state == ISOLATED)
		{
			if (port->AdminEdge || port->AutoEdge || !port->isolate || !port->operPointToPointMAC)
			{
				return NOT_EDGE;
			}

			return (State)0;
		}

		assert (false);
		return (State)0;
	}

	// ============================================================================

	static void InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, State state, unsigned int timestamp)
	{
		assert (givenPort != -1);
		assert (givenTree == -1);

		PORT* port = bridge->ports [givenPort];
	
		if (state == EDGE)
		{
			port->operEdge = true;
			port->isolate = false;
		}
		else if (state == NOT_EDGE)
		{
			port->operEdge = false;
			port->isolate = false;
		}
		else if (state == ISOLATED)
		{
			port->operEdge = false;
			port->isolate = true;
		}
		else
			assert (false);
	}
};

const SM_INFO<BridgeDetection::State> BridgeDetection::sm = 
{
	"BridgeDetection",
	&BridgeDetectionImpl::GetStateName,
	&BridgeDetectionImpl::CheckConditions,
	&BridgeDetectionImpl::InitState,
};

