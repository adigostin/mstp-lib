
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_PORT_H
#define MSTP_LIB_PORT_H

#include "stp_base_types.h"
#include "stp_bpdu.h"
#include "stp.h"

// All references are to 802.1Q-2011

// 13.25
struct PORT_TREE
{
	bool pseudoRootId:1;	// 13.25.ad) - 13.25.37
	bool agree		: 1;	// 13.25.ae) - 13.25.3
	bool agreed		: 1;	// 13.25.af) - 13.25.4
	bool disputed	: 1;	// 13.25.ai) - 13.25.9
	bool fdbFlush	: 1;	// 13.25.aj) - 13.25.13
	bool forward	: 1;	// 13.25.ak) - 13.25.14
	bool learn		: 1;	// 13.25.ao) - 13.25.21
	bool learning	: 1;	// 13.25.ap) - 13.25.13
	bool forwarding	: 1;	// 13.25.al) - 13.25.15
	bool proposed	: 1;	// 13.25.av) - 13.25.35
	bool proposing	: 1;	// 13.25.aw) - 13.25.36
	bool rcvdMsg	: 1;	// 13.25.ay) - 13.25.41
	bool rcvdTc		: 1;	// 13.25.az) - 13.25.44
	bool reRoot		: 1;	// 13.25.ba) - 13.25.47
	bool reselect	: 1;	// 13.25.bb) - 13.25.48
	bool selected	: 1;	// 13.25.bd) - 13.25.52
	bool sync		: 1;	// 13.25.bf) - 13.25.55
	bool synced		: 1;	// 13.25.bg) - 13.25.56
	bool tcProp		: 1;	// 13.25.bh) - 13.25.58
	bool updtInfo	: 1;	// 13.25.bi) - 13.25.61

	// Note AG: I added these flag variables here for convenience; they're not in the standard.
	// When a BPDU is received, the procedure rcvMsgs() populates them with the flags present in the received BPDU.
	bool          msgFlagsTc             : 1;
	bool          msgFlagsProposal       : 1;
	unsigned char msgFlagsPortRole       : 2;
	bool          msgFlagsLearning       : 1;
	bool          msgFlagsForwarding     : 1;
	bool          msgFlagsAgreement      : 1;
	bool          msgFlagsTcAckOrMaster  : 1;

	INFO_IS			infoIs		: 8;	// 13.25.am) - 13.25.17
	RCVD_INFO		rcvdInfo	: 8;	// 13.25.ax) - 13.25.39
	STP_PORT_ROLE	role		: 8;	// 13.25.bc) - 13.25.51
	STP_PORT_ROLE	selectedRole: 8;	// 13.25.be) - 13.25.53

	unsigned int InternalPortPathCost;	// 13.25.an) - 13.25.18

	PRIORITY_VECTOR designatedPriority; // 13.25.ag) - 13.25.7
	PRIORITY_VECTOR msgPriority;		// 13.25.aq) - 13.25.26
	PRIORITY_VECTOR portPriority;		// 13.25.at) - 13.25.33

	TIMES	designatedTimes;// 13.25.ah) - 13.25.8
	TIMES	msgTimes;		// 13.25.ar) - 13.25.27
	TIMES	portTimes;		// 13.25.au) - 13.25.34

	PORT_ID portId;			// 13.25.as) - 13.25.32

	// 13.23 State machine timers
	unsigned short fdWhile;			// e) - 13.23.2
	unsigned short rrWhile;			// f) - 13.23.7
	unsigned short rbWhile;			// g) - 13.23.5
	unsigned short tcWhile;			// h) - 13.23.9
	unsigned short rcvdInfoWhile;	// i) - 13.23.6
	unsigned short tcDetected;		// j) - 13.23.8
};

// ============================================================================

// 13.25
struct PORT
{
	// There is one instance per port of each of the following variables:
	bool	AdminEdge;		// 13.25.a) - 13.25.1
	unsigned int ageingTime;// 13.25.b) - 13.25.2
	bool	AutoEdge;		// 13.25.c) - 13.25.5
	bool	AutoIsolate;	// 13.25.d) - 13.25.6
	bool	enableBPDUrx;	// 13.25.e) - 13.25.10
	bool	enableBPDUtx;	// 13.25.f) - 13.25.11
	bool	isL2gp;			// 13.25.g) - 13.25.19
	bool	isolate;		// 13.25.h) - 13.25.20
	bool	mcheck;			// 13.25.i) - 13.25.25
	bool	newInfo;		// 13.25.j) - 13.25.28
	bool	operEdge;		// 13.25.k) - 13.25.30
	bool	portEnabled;	// 13.25.l) - 13.25.31
	bool	rcvdBpdu;		// 13.25.m) - 13.25.38
	bool	rcvdRSTP;		// 13.25.n) - 13.25.42
	bool	rcvdSTP;		// 13.25.o) - 13.25.43
	bool	rcvdTcAck;		// 13.25.p) - 13.25.45
	bool	rcvdTcn;		// 13.25.q) - 13.25.46
	bool	restrictedRole;	// 13.25.r) - 13.25.49
	bool	restrictedTcn;	// 13.25.s) - 13.25.50
	bool	sendRSTP;		// 13.25.t) - 13.25.54
	bool	tcAck;			// 13.25.u) - 13.25.57
	bool	tick;			// 13.25.v) - 13.25.59
	unsigned short txCount;	// 13.25.w) - 13.25.60

	// If MSTP is implemented there is one instance per port, applicable to the CIST and to all MSTIs, of the following variable(s):
	bool	rcvdInternal;	// 13.25.x) - 13.25.40

	unsigned int AdminExternalPortPathCost; // Not in the standard. Stores ieee8021SpanningTreeRstpPortAdminPathCost / ieee8021MstpCistPortAdminPathCost

	// If MSTP is implemented there is one instance per port of each of the following variables for the CIST:
	unsigned int ExternalPortPathCost;	// 13.25.y) - 13.25.12
	bool	infoInternal;	// 13.25.z) - 13.25.16
	bool	master;			// 13.25.aa) - 13.25.23
	bool	mastered;		// 13.25.ab) - 13.25.24

	// A single per port instance of the following variable(s) applies to all MSTIs:
	bool	newInfoMsti;	// 13.25.ac) - 13.25.29

	// 13.23 State machine timers
	// One instance of the following shall be implemented per port:
	unsigned short mDelayWhile;		// a) - 13.23.1
	unsigned short helloWhen;		// b) - 13.23.3
	unsigned short edgeDelayWhile;	// c) - 13.23.4
	// One instance of the following shall be implemented per port when L2GP functionality is provided:
	unsigned short pseudoInfoHelloWhen; // d) - 13.23.10

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
	bool detectedPointToPointMAC;
};

#endif
