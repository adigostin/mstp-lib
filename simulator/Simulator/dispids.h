
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once

#define dispidBridges      1
#define dispidWires        2
#define dispidPortTrees    3
#define dispidStpEnabled   4
#define dispidBridgeWidth  5
#define dispidBridgeHeight 6
#define dispidPortAdminP2P 7
#define dispidPortOperP2P  8
#define dispidBridgePrio   9
#define dispidAdminInternalPortPathCost 10
#define dispidInternalPortPathCost      11
#define dispidMigrateTime               12
#define dispidBridgeHelloTime           13
#define dispidBridgeMaxAge              14
#define dispidBridgeForwardDelay        15
#define dispidTxHoldCount               16
#define dispidMaxHops                   17
#define dispidBridgeAddress             18
#define dispidStpVersion                19
#define dispidPortCount    20
#define dispidMstiCount    21
#define dispidMstConfigName      22
#define dispidMstConfigRevLevel  23
#define dispidMstConfigTable     24
#define dispidPortLearning   25
#define dispidPortForwarding 26
#define dispidPortRole       27
#define dispidPortPriority   28
#define dispidSupportedSpeed 29
#define dispidWireEndP0 30
#define dispidWireEndP1 31
#define dispidWireEndBridgeIndex 32
#define dispidWireEndPortIndex   33
#define dispidWireEndX           34
#define dispidWireEndY           35
#define dispidBridgeX  36
#define dispidBridgeY  37
#define dispidTopologyChangeCount 38
#define dispidAdminEdge           39
#define dispidOperEdge            40
#define dispidPorts               41
#define dispidBridgeTrees         42
#define dispidPortSide            43
#define dispidPortOffset          44
#define dispidPortActualSpeed     45
#define dispidPortMacOperational  46
#define dispidPortDetectedP2P     47
// List continues in Simulator.idl (the above will be moved there eventually).

#ifdef __midl
	#define guidMSTConfigIdEditor    533D2CFB-2CE4-46EF-97EC-D8126972EEEA
#else
	DEFINE_GUID (guidMSTConfigIdEditor,    0x533D2CFB, 0x2CE4, 0x46EF, 0x97, 0xEC, 0xD8, 0x12, 0x69, 0x72, 0xEE, 0xEA);
#endif