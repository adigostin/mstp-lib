
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "../stp_bridge.h"

const char*	PortTimers_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortTimers_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortTimers_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortProtocolMigration_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortProtocolMigration_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortProtocolMigration_802_1Q_2011_InitState			(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortReceive_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortReceive_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortReceive_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	BridgeDetection_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	BridgeDetection_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		BridgeDetection_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortInformation_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortInformation_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortInformation_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortRoleSelection_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortRoleSelection_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortRoleSelection_802_1Q_2011_InitState			(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortRoleTransitions_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortRoleTransitions_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortRoleTransitions_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortStateTransition_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortStateTransition_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortStateTransition_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	PortTransmit_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	PortTransmit_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		PortTransmit_802_1Q_2011_InitState			(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char*	TopologyChange_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE	TopologyChange_802_1Q_2011_CheckConditions	(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void		TopologyChange_802_1Q_2011_InitState		(STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

const char* L2GP_Receive_802_1Q_2011_GetStateName (SM_STATE state);
SM_STATE    L2GP_Receive_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state);
void        L2GP_Receive_802_1Q_2011_InitState       (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp);

static const SM_INFO smInfo [] =
{
	{
		SM_INFO::PER_PORT,
		"PortTimers",
		PortTimers_802_1Q_2011_GetStateName,
		PortTimers_802_1Q_2011_CheckConditions,
		PortTimers_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT,
		"PortProtocolMigration",
		PortProtocolMigration_802_1Q_2011_GetStateName,
		PortProtocolMigration_802_1Q_2011_CheckConditions,
		PortProtocolMigration_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT,
		"PortReceive",
		PortReceive_802_1Q_2011_GetStateName,
		PortReceive_802_1Q_2011_CheckConditions,
		PortReceive_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT,
		"BridgeDetection",
		BridgeDetection_802_1Q_2011_GetStateName,
		BridgeDetection_802_1Q_2011_CheckConditions,
		BridgeDetection_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT_PER_TREE,
		"PortInformation",
		PortInformation_802_1Q_2011_GetStateName,
		PortInformation_802_1Q_2011_CheckConditions,
		PortInformation_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_BRIDGE_PER_TREE,
		"PortRoleSelection",
		PortRoleSelection_802_1Q_2011_GetStateName,
		PortRoleSelection_802_1Q_2011_CheckConditions,
		PortRoleSelection_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT_PER_TREE,
		"PortRoleTransitions",
		PortRoleTransitions_802_1Q_2011_GetStateName,
		PortRoleTransitions_802_1Q_2011_CheckConditions,
		PortRoleTransitions_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT_PER_TREE,
		"PortStateTransition",
		PortStateTransition_802_1Q_2011_GetStateName,
		PortStateTransition_802_1Q_2011_CheckConditions,
		PortStateTransition_802_1Q_2011_InitState
	},

	{
		SM_INFO::PER_PORT_PER_TREE,
		"TopologyChange",
		TopologyChange_802_1Q_2011_GetStateName,
		TopologyChange_802_1Q_2011_CheckConditions,
		TopologyChange_802_1Q_2011_InitState
	},

	// This state machine is optional and we're not using it for now.
	// L2GP requires various STP variables to be set to work properly (isL2gp, pseudoRootId etc.)
	//{
	//	SM_INFO::PER_PORT,
	//	"L2GP_Receive",
	//	L2GP_Receive_802_1Q_2011_GetStateName,
	//	L2GP_Receive_802_1Q_2011_CheckConditions,
	//	L2GP_Receive_802_1Q_2011_InitState,
	//},
};

static const SM_INFO transmitSmInfo =
{
	SM_INFO::PER_PORT,
	"PortTransmit",
	PortTransmit_802_1Q_2011_GetStateName,
	PortTransmit_802_1Q_2011_CheckConditions,
	PortTransmit_802_1Q_2011_InitState
};

const SM_INTERFACE smInterface_802_1Q_2011 =
{
	smInfo,
	sizeof (smInfo) / sizeof (SM_INFO),
	&transmitSmInfo
};
