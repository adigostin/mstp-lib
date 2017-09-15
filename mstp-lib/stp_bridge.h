
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_BRIDGE_H
#define MSTP_LIB_BRIDGE_H

// All references of the kind XX.YY are to sections in 802.1Q-2011 pdf.

#include "stp_base_types.h"
#include "stp_port.h"

typedef const char* (*SM_GET_STATE_NAME) (SM_STATE state);
typedef SM_STATE (*SM_CHECK_CONDITIONS) (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
typedef void (*SM_INIT_STATE) (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

struct SM_INFO
{
	enum INSTANCE_TYPE
	{
		PER_BRIDGE,
		PER_BRIDGE_PER_TREE,
		PER_PORT,
		PER_PORT_PER_TREE,
	};

	INSTANCE_TYPE instanceType;

	const char* smName;

	SM_GET_STATE_NAME getStateName;
	SM_CHECK_CONDITIONS checkConditions;
	SM_INIT_STATE initState;
};

struct SM_INTERFACE
{
	const SM_INFO* smInfo;
	unsigned int smInfoCount;

	const SM_INFO* transmitSmInfo;
};

extern const SM_INTERFACE smInterface_802_1Q_2011;

// ============================================================================

// 13.24
struct BRIDGE_TREE
{
	// There is one instance per bridge of each of the following for the CIST, and one for each MSTI.
private:
	// TODO: make the address field of BridgeIdentifier common to all trees
	BRIDGE_ID				BridgeIdentifier;	// 13.24.e) - 13.24.1
	PRIORITY_VECTOR			BridgePriority;		// 13.24.f) - 13.24.2

public:
	TIMES					BridgeTimes;		// 13.24.g) - 13.24.3
	PORT_ID					rootPortId;			// 13.24.h) - 13.24.7
	PRIORITY_VECTOR			rootPriority;		// 13.24.i) - 13.24.8
	TIMES					rootTimes;			// 13.24.j) - 13.24.9

private:
	void UpdateBridgePriorityFromBridgeIdentifier ()
	{
		// Note: ExternalRootPathCost, InternalRootPathCost and DesignatedPortId are always zero
		// in the BridgePriority field, so there's no need to assign them.

		unsigned int treeIndex = BridgeIdentifier.GetPriority () & 0x0FFF;
		if (treeIndex == CIST_INDEX)
		{
			BridgePriority.RootId = BridgeIdentifier;
			//BridgePriority.ExternalRootPathCost = 0;
		}

		BridgePriority.RegionalRootId = BridgeIdentifier;
		//BridgePriority.InternalRootPathCost = 0;
		BridgePriority.DesignatedBridgeId = BridgeIdentifier;
		//BridgePriority.DesignatedPortId = 0;
	}

public:
	const BRIDGE_ID& GetBridgeIdentifier () const
	{
		return BridgeIdentifier;
	}

	void SetBridgeIdentifier (const BRIDGE_ID& newBridgeIdentifier)
	{
		BridgeIdentifier = newBridgeIdentifier;
		UpdateBridgePriorityFromBridgeIdentifier ();
	}

	void SetBridgeIdentifier (unsigned short settablePriorityComponent, unsigned short treeIndex, const unsigned char address[6])
	{
		BridgeIdentifier.Set (settablePriorityComponent, treeIndex, address);
		UpdateBridgePriorityFromBridgeIdentifier ();
	}

	const PRIORITY_VECTOR& GetBridgePriority () const
	{
		return this->BridgePriority;
	}
};

// ============================================================================

// 13.23
struct STP_BRIDGE
{
	static const unsigned int LogIndentSize = 2;

	char* logBuffer;
	unsigned int logBufferMaxSize;
	unsigned int logBufferUsedSize;
	unsigned int logIndent;
	bool logLineStarting;
	bool loggingEnabled;
	bool BEGIN; // 13.23.1
	bool started;
	int logCurrentPort;
	int logCurrentTree;

	STP_CALLBACKS callbacks;

	unsigned int portCount;
	unsigned int mstiCount;
	unsigned int maxVlanNumber;

	unsigned int treeCount() const { return 1 + ((ForceProtocolVersion >= STP_VERSION_MSTP) ? mstiCount : 0); }

	BRIDGE_TREE** trees;
	PORT** ports;
	INV_UINT2* mstConfigTable;

	// 13.24 Per bridge variables
	// There is one instance per bridge component of the following variable(s):
	STP_VERSION ForceProtocolVersion;				// 13.24.a) - 13.24.4
	static const unsigned int TxHoldCount = 6;		// 13.24.b) - 13.24.10
	static const unsigned short MigrateTime = 3;	// 13.24.c) - 13.24.5
	STP_MST_CONFIG_ID MstConfigId;					// 13.24.d) - 13.24.6
	// The above parameters ((a) through (d)) are not modified by the operation of the spanning tree protocols, but
	// are treated as constants by the state machines. If ForceProtocolVersion or MSTConfigId are modified by
	// management, BEGIN shall be asserted for all state machines.

	// From Table 13-5 on page 356 in 802.1Q-2011
	static const unsigned short BridgeHelloTime = 2;
	static const unsigned short BridgeMaxAge = 20;		// 13.22.i in 802.1Q-2005 --- 17.14 in 802.1D-2004
	static const unsigned short BridgeForwardDelay = 15;// 13.22.f in 802.1Q-2005 --- 17.14 in 802.1D-2004
	static const unsigned short MaxHops = 20;			// 13.22.1 in 802.1Q-2005 --- 13.23.7 --- 13.37.3

	// Not from the standard. See long comment in 802_1Q_2011_procedures.cpp, just above CallTcCallback().
	static const unsigned short TcIgnoreMax = 6;
	unsigned short tcIgnore;

	void* applicationContext;

	SM_STATE* states;

	const SM_INTERFACE* smInterface;

	// This variable is supposed to be be accessed only while a received BPDU is being handled.
	// When there's no received BPDU, we set it to the invalid value NULL, to cause a crash on access and signal the programming error early.
	// (Note that the crash won't happen on some microcontrollers for which address 0 is
	//  readable/writeable, that's why we also have asserts all around the place).
	const MSTP_BPDU*		receivedBpduContent;
	VALIDATED_BPDU_TYPE		receivedBpduType;
};



#endif
