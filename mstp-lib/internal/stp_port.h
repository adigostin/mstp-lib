
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

// This file implements "13.27 Per Port variables" from 802.1Q-2018.

#ifndef MSTP_LIB_PORT_H
#define MSTP_LIB_PORT_H

#include "stp_base_types.h"
#include "stp_bpdu.h"
#include "stp_sm.h"
#include "../stp.h"

struct PORT_TREE
{
	BRIDGE_ID pseudoRootId; // 13.27.ae) - 13.27.51
	bool agree      : 1; // 13.27.ap) - 13.27.3
	bool agreed     : 1; // 13.27.aq) - 13.27.4
	bool disputed   : 1; // 13.27.at) - 13.27.22
	bool fdbFlush   : 1; // 13.27.au) - 13.27.28
	bool forward    : 1; // 13.27.av) - 13.27.29
	bool forwarding : 1; // 13.27.aw) - 13.27.30
	bool learn      : 1; // 13.27.az) - 13.27.34
	bool learning   : 1; // 13.27.ba) - 13.27.35
	bool proposed   : 1; // 13.27.bg) - 13.27.49
	bool proposing  : 1; // 13.27.bh) - 13.27.50
	bool rcvdMsg    : 1; // 13.27.bj) - 13.27.55
	bool rcvdTc     : 1; // 13.27.bk) - 13.27.58
	bool reRoot     : 1; // 13.27.bl) - 13.27.61
	bool reselect   : 1; // 13.27.bm) - 13.27.62
	bool selected   : 1; // 13.27.bo) - 13.27.67
	bool sync       : 1; // 13.27.bq) - 13.27.70
	bool synced     : 1; // 13.27.br) - 13.27.71
	bool tcProp     : 1; // 13.27.bs) - 13.27.73
	bool updtInfo   : 1; // 13.27.bt) - 13.27.76

	// Note AG: I added these flag variables here for convenience; they're not in the standard.
	// When a BPDU is received, the procedure rcvMsgs() populates them with the flags present in the received BPDU.
	bool          msgFlagsTc            : 1;
	bool          msgFlagsProposal      : 1;
	unsigned char msgFlagsPortRole      : 2;
	bool          msgFlagsLearning      : 1;
	bool          msgFlagsForwarding    : 1;
	bool          msgFlagsAgreement     : 1;
	bool          msgFlagsTcAckOrMaster : 1;

	INFO_IS       infoIs;       // 13.27.ax) - 13.27.32
	RCVD_INFO     rcvdInfo;	    // 13.27.bi) - 13.27.53
	STP_PORT_ROLE role;	        // 13.27.bn) - 13.27.66
	STP_PORT_ROLE selectedRole;	// 13.27.bp) - 13.27.68

	unsigned int InternalPortPathCost; // 13.27.ay) - 13.27.33

	PRIORITY_VECTOR designatedPriority; // 13.27.ar) - 13.27.20
	PRIORITY_VECTOR msgPriority;        // 13.27.bb) - 13.27.39
	PRIORITY_VECTOR portPriority;       // 13.27.be) - 13.27.47

	TIMES designatedTimes; // 13.27.as) - 13.27.21
	TIMES msgTimes;        // 13.27.bc) - 13.27.40
	TIMES portTimes;       // 13.27.bf) - 13.27.48

	PORT_ID portId; // 13.27.bd) - 13.27.46

	// If the ISIS-SPB is implemented, there is one instance per port of the following variable(s) for the CIST and
	// one per port for each SPT:
	PRIORITY_VECTOR agreedPriority; // 13.27.bu) - 13.27.13
	bool agreementOutstanding;      // 13.27.bv) - 13.27.15

	// If the ISIS-SPB is implemented, there is one instance per port of the following variables for each SPT:
	bool            agreedAbove;       // 13.27.bw) - 13.27.5
	PRIORITY_VECTOR neighbourPriority; // 13.27.bx) - 13.27.41

	// 13.25 State machine timers
	unsigned short fdWhile;			// e) - 13.25.2
	unsigned short rrWhile;			// f) - 13.25.7
	unsigned short rbWhile;			// g) - 13.25.5
	unsigned short tcWhile;			// h) - 13.25.9
	unsigned short rcvdInfoWhile;	// i) - 13.25.6
	unsigned short tcDetected;		// j) - 13.25.8

	PortInformation::State     portInformationState;
	PortRoleTransitions::State portRoleTransitionsState;
	PortStateTransition::State portStateTransitionState;
	TopologyChange::State      topologyChangeState;
};

struct PORT
{
	// There is one instance per port of each of the following variables:
	bool AdminEdge;      // 13.27.a) - 13.27.1
	unsigned short ageingTime; // 13.27.b) - 13.27.2
	bool AutoEdge;       // 13.27.c) - 13.27.18
	bool AutoIsolate;    // 13.27.d) - 13.27.19
	bool enableBPDUrx;   // 13.27.e) - 13.27.23
	bool enableBPDUtx;   // 13.27.f) - 13.27.24
	unsigned int ExternalPortPathCost; // 13.27.g) - 13.27.25
	bool isL2gp;         // 13.27.h) - 13.27.26
	bool isolate;        // 13.27.i) - 13.27.27
	bool mcheck;         // 13.27.j) - 13.27.38
	bool newInfo;        // 13.27.k) - 13.27.42
	bool operEdge;       // 13.27.l) - 13.27.44
	bool portEnabled;    // 13.27.m) - 13.27.45
	bool rcvdBpdu;       // 13.27.n) - 13.27.52
	bool rcvdRSTP;       // 13.27.o) - 13.27.56
	bool rcvdSTP;        // 13.27.p) - 13.27.57
	bool rcvdTcAck;      // 13.27.q) - 13.27.59
	bool rcvdTcn;        // 13.27.r) - 13.27.60
	bool restrictedRole; // 13.27.s) - 13.27.64
	bool restrictedTcn;  // 13.27.t) - 13.27.65
	bool sendRSTP;       // 13.27.u) - 13.27.69
	bool tcAck;          // 13.27.v) - 13.27.72
	bool tick;           // 13.27.w) - 13.27.74
	unsigned short txCount; // 13.27.x) - 13.27.75

	// If MSTP or the ISIS-SPB is implemented, there is one instance per port, applicable to the CIST and to all
	// MSTIs and SPTs, of the following variable(s):
	bool rcvdInternal; // 13.27.y) - 13.27.54
	bool restrictedDomainRole; // 13.27.z) - 13.27.63

	// If MSTP or the ISIS-SPB is implemented, there is one instance per port of each of the following variables for the CIST:
	bool infoInternal; // 13.27.aa) - 13.27.31
	bool master;       // 13.27.ab) - 13.27.36
	bool mastered;     // 13.27.ac) - 13.27.37

	// A single per port instance of the following variable(s) applies to all MSTIs:
	bool newInfoMsti; // 13.27.ad) - 13.27.43

	// If the ISIS-SPB is implemented, there is one instance per port of the following variable(s):
	bool agreedMisorder;    // 13.27.af) - 13.27.10
	unsigned char agreedN;  // 13.27.ag) - 13.27.11
	unsigned char agreedND; // 13.27.ah) - 13.27.12
	unsigned char agreeN;   // 13.27.ai) - 13.27.16
	unsigned char agreeND;  // 13.27.aj) - 13.27.17

	// If the ISIS-SPB is implemented, there is one instance per port of the following variable(s), with that single
	// instance supporting all SPTs:
	int  agreedDigest;      // 13.27.ak) - 13.27.6
	bool agreedDigestValid; // 13.27.al) - 13.27.7
	bool agreeDigest;       // 13.27.am) - 13.27.8
	bool agreeDigestValid;  // 13.27.an) - 13.27.9
	bool agreedTopology;    // 13.27.ao) - 13.27.14

	// 13.25 State machine timers
	// One instance of the following shall be implemented per port:
	unsigned short edgeDelayWhile; // a) - 13.25.1
	unsigned short helloWhen;      // b) - 13.25.3
	unsigned short mDelayWhile;    // c) - 13.25.4
	// One instance of the following shall be implemented per port when L2GP functionality is provided:
	unsigned short pseudoInfoHelloWhen; // d) - 13.25.10




	PORT_TREE** trees;

	STP_ADMIN_P2P adminPointToPointMAC;

	// TODO: we might have to force operPointToPointMAC to false while a port is disabled,
	// to avoid an infinite loop in the BridgeDetection state machine. Also see the comments there.
	bool	operPointToPointMAC;

	// Variable that stores the detectedPointToPointMAC parameter passed to STP_OnPortEnabled.
	// It reflects the P2P status as detected by the hardware, not affected when setting adminPointToPointMAC.
	// Not in the standard. I introduced because it's needed in the following scenario:
	//  - application calls STP_SetAdminP2P(FORCE_TRUE) => the library will set operPointToPointMAC to true.
	//  - application calls STP_OnPortEnabled (pointToPointMAC = XXX) => the library will write XXX to this variable and keep operPointToPointMAC true.
	//  - application calls STP_SetAdminP2P(AUTO) => the library must set operPointToPointMAC to XXX, which it reads from this variable.
	// See also detectedExternalPortPathCost.
	bool detectedPointToPointMAC;

	// Not in the standard. Stores the path cost calculated in STP_OnPortEnabled, in case the user later resets the admin cost.
	// See also detectedPointToPointMAC.
	unsigned int detectedPortPathCost;

	// Not in the standard. Stores ieee8021SpanningTreeRstpPortAdminPathCost / ieee8021MstpCistPortAdminPathCost
	unsigned int adminExternalPortPathCost;

	PortTimers::State            portTimersState;
	PortProtocolMigration::State portProtocolMigrationState;
	PortReceive::State           portReceiveState;
	BridgeDetection::State       bridgeDetectionState;
	L2GPortReceive::State        l2gpState;
	PortTransmit::State          portTransmitState;
};

#endif
