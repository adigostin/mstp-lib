
// This file is part of the mstp-lib library, available at http://sourceforge.net/projects/mstp-lib/
// Copyright (c) 2011-2014 Adrian Gostin, distributed under the GNU General Public License v3.

// Find documentation for all STP functions in the _help directory in the source code tree.
//
// Improvements and bug fixes may have been made in the source code, but not put up yet as a zip download;
// Have a look at the latest source code on the Sourceforge website to learn about these.
//
// Also check the open tickets to learn about known issues: http://sourceforge.net/p/mstp-lib/tickets

#ifndef MSTP_LIB_H
#define MSTP_LIB_H

#ifndef __cplusplus
#error Trying to include stp.h from C, which is not supported. Try using C++.
#endif

struct STP_BRIDGE;

enum STP_FLUSH_FDB_TYPE
{
	STP_FLUSH_FDB_TYPE_IMMEDIATE,
	STP_FLUSH_FDB_TYPE_RAPID_AGEING,
};

typedef void  (*STP_CALLBACK_ENABLE_LEARNING)				(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
typedef void  (*STP_CALLBACK_ENABLE_FORWARDING)				(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
typedef void* (*STP_CALLBACK_TRANSMIT_GET_BUFFER)			(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
typedef void  (*STP_CALLBACK_TRANSMIT_RELEASE_BUFFER)		(STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
typedef void  (*STP_CALLBACK_FLUSH_FDB)						(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
typedef void  (*STP_CALLBACK_DEBUG_STR_OUT)					(STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, bool flush);
typedef void  (*STP_CALLBACK_ON_TOPOLOGY_CHANGE)			(STP_BRIDGE* bridge);
typedef void  (*STP_CALLBACK_ON_NOTIFIED_TOPOLOGY_CHANGE)	(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);
typedef void* (*STP_CALLBACK_ALLOC_AND_ZERO_MEMORY) (unsigned int size);
typedef void  (*STP_CALLBACK_FREE_MEMORY) (void* p);

struct STP_CALLBACKS
{
	STP_CALLBACK_ENABLE_LEARNING			enableLearning;
	STP_CALLBACK_ENABLE_FORWARDING			enableForwarding;
	STP_CALLBACK_TRANSMIT_GET_BUFFER		transmitGetBuffer;
	STP_CALLBACK_TRANSMIT_RELEASE_BUFFER	transmitReleaseBuffer;
	STP_CALLBACK_FLUSH_FDB					flushFdb;
	STP_CALLBACK_DEBUG_STR_OUT				debugStrOut;
	STP_CALLBACK_ON_TOPOLOGY_CHANGE			onTopologyChange;
	STP_CALLBACK_ON_NOTIFIED_TOPOLOGY_CHANGE onNotifiedTopologyChange;
	STP_CALLBACK_ALLOC_AND_ZERO_MEMORY		allocAndZeroMemory;
	STP_CALLBACK_FREE_MEMORY				freeMemory;
};

// 6.6.3 Point-to-point parameters
enum STP_ADMIN_P2P
{
	// There's logic in the STP library that counts on AUTO being equal to 0, so don't move it from the top of the enumeration.
	STP_ADMIN_P2P_AUTO,
	STP_ADMIN_P2P_FORCE_TRUE,
	STP_ADMIN_P2P_FORCE_FALSE,
};

enum STP_VERSION
{
	STP_VERSION_LEGACY_STP = 0,
	STP_VERSION_RSTP = 2,
	STP_VERSION_MSTP = 3,
};

enum STP_PORT_ROLE
{
	STP_PORT_ROLE_UNKNOWN	= 0,
	STP_PORT_ROLE_DISABLED	= 5,
	STP_PORT_ROLE_ROOT		= 6,
	STP_PORT_ROLE_DESIGNATED= 7,
	STP_PORT_ROLE_ALTERNATE = 8,
	STP_PORT_ROLE_BACKUP    = 9,
	STP_PORT_ROLE_MASTER	= 10,
};

STP_BRIDGE* STP_CreateBridge (unsigned int portCount,
						  unsigned int treeCount,
						  const STP_CALLBACKS* callbacks,
						  STP_VERSION protocolVersion,
						  const unsigned char bridgeAddress [6],
						  unsigned int debugLogBufferSize);
void STP_DestroyBridge (STP_BRIDGE* bridge);

void STP_StartBridge (STP_BRIDGE* bridge, unsigned int timestamp);
void STP_StopBridge (STP_BRIDGE* bridge, unsigned int timestamp);
bool STP_IsBridgeStarted (STP_BRIDGE* bridge);

void STP_EnableLogging (STP_BRIDGE* bridge, bool enable);
bool STP_IsLoggingEnabled (STP_BRIDGE* bridge);

unsigned int STP_GetPortCount (STP_BRIDGE* bridge);
unsigned int STP_GetTreeCount (STP_BRIDGE* bridge);

enum STP_VERSION STP_GetStpVersion (STP_BRIDGE* bridge);
void STP_SetStpVersion (STP_BRIDGE* bridge, enum STP_VERSION version);

// Call this when you receive a BPDU.
void STP_OnBpduReceived (STP_BRIDGE* bridge, unsigned int portIndex, const unsigned char* bpdu, unsigned int bpduSize, unsigned int timestamp);

// Call this every time the bridge's MAC address changes during bridge operation.
void STP_SetBridgeAddress (STP_BRIDGE* bridge, const unsigned char* address, unsigned int timestamp);
void STP_GetBridgeAddress (STP_BRIDGE* bridge, unsigned char* addressOut6Bytes);

// Call these whenever one of the ports your ports changes state. See 13.25.31 portEnabled in 802.1Q-2011 for details.
void STP_OnPortEnabled (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int speedMegabitsPerSecond, bool detectedPointToPointMAC, unsigned int timestamp);
void STP_OnPortDisabled (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int timestamp);

void STP_OnOneSecondTick (STP_BRIDGE* bridge, unsigned int timestamp);

// 0-61440 in steps of 4096
void           STP_SetBridgePriority (STP_BRIDGE* bridge, unsigned int treeIndex, unsigned short bridgePriority, unsigned int timestamp);
unsigned short STP_GetBridgePriority (STP_BRIDGE* bridge, unsigned int treeIndex);

// 0-240 in steps of 16
void          STP_SetPortPriority (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned char portPriority, unsigned int timestamp);
unsigned char STP_GetPortPriority (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);
unsigned short STP_GetPortIdentifier (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);

void STP_SetPortAdminEdge (STP_BRIDGE* bridge, unsigned int portIndex, bool adminEdge, unsigned int timestamp);
bool STP_GetPortAdminEdge (STP_BRIDGE* bridge, unsigned int portIndex);

void STP_SetPortAutoEdge (STP_BRIDGE* bridge, unsigned int portIndex, bool autoEdge, unsigned int timestamp);
bool STP_GetPortAutoEdge (STP_BRIDGE* bridge, unsigned int portIndex);

void          STP_SetPortAdminPointToPointMAC (STP_BRIDGE* bridge, unsigned int portIndex, STP_ADMIN_P2P adminPointToPointMAC, unsigned int timestamp);
STP_ADMIN_P2P STP_GetPortAdminPointToPointMAC (STP_BRIDGE* bridge, unsigned int portIndex);

bool STP_GetPortEnabled				(STP_BRIDGE* bridge, unsigned int portIndex);
STP_PORT_ROLE STP_GetPortRole		(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);
bool STP_GetPortLearning			(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);
bool STP_GetPortForwarding			(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);
bool STP_GetPortOperEdge			(STP_BRIDGE* bridge, unsigned int portIndex);
bool STP_GetPortOperPointToPointMAC	(STP_BRIDGE* bridge, unsigned int portIndex);

void STP_GetMstConfigName (STP_BRIDGE* bridge, char nameOut [33]);
void STP_SetMstConfigName (STP_BRIDGE* bridge, const char* name, unsigned int timestamp);


void STP_SetMstConfigRevisionLevel (STP_BRIDGE* bridge, unsigned short revisionLevel, unsigned int timestamp);
unsigned short STP_GetMstConfigRevisionLevel (STP_BRIDGE* bridge);

struct VLAN_TO_MSTID
{
	unsigned char vlanLow;
	unsigned char vlanHigh;
	unsigned char mstid;
};

void STP_SetMstConfigTableAndComputeDigest1 (STP_BRIDGE* bridge, const unsigned char mstids [4094], unsigned int timestamp);
void STP_SetMstConfigTableAndComputeDigest  (STP_BRIDGE* bridge, const struct VLAN_TO_MSTID* table, unsigned int tableEntryCount, unsigned int timestamp);
void STP_GetMstConfigTable (STP_BRIDGE* bridge, unsigned char mstidsOut [4094]);
const unsigned char* STP_GetMstConfigTableDigest (STP_BRIDGE* bridge);
unsigned char STP_GetTreeIndexFromVlanNumber (STP_BRIDGE* bridge, unsigned short vlanNumber);

const char* STP_GetPortRoleString (STP_PORT_ROLE portRole);
const char* STP_GetVersionString (enum STP_VERSION version);
const char* STP_GetAdminP2PString (enum STP_ADMIN_P2P adminP2P);

void STP_GetRootPriorityVector (STP_BRIDGE* bridge, unsigned int treeIndex, unsigned char* priorityVector36BytesOut);
void STP_GetRootTimes (STP_BRIDGE* bridge,
					   unsigned int treeIndex,
					   unsigned short* forwardDelayOutOrNull,
					   unsigned short* helloTimeOutOrNull,
					   unsigned short* maxAgeOutOrNull,
					   unsigned short* messageAgeOutOrNull,
					   unsigned char* remainingHopsOutOrNull);
		
void  STP_SetApplicationContext (STP_BRIDGE* bridge, void* applicationContext);
void* STP_GetApplicationContext (STP_BRIDGE* bridge);

#endif
