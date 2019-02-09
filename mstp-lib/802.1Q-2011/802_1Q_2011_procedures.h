
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_802_1Q_2011_PROCEDURES_H
#define MSTP_LIB_802_1Q_2011_PROCEDURES_H

#include "../stp_base_types.h"

// 13.27 State machine procedures

bool betterorsameInfo	(STP_BRIDGE*, int givenPort, int givenTree, INFO_IS newInfoIs); // 13.27.a) - 13.27.1
void clearAllRcvdMsgs	(STP_BRIDGE*, int givenPort); // 13.27.b) - 13.27.2
void clearReselectTree	(STP_BRIDGE*, int givenTree); // 13.27.c) - 13.27.3
void disableForwarding	(STP_BRIDGE*, int givenPort, int givenTree, unsigned int timestamp);	// 13.27.d) - 13.27.4
void disableLearning	(STP_BRIDGE*, int givenPort, int givenTree, unsigned int timestamp);	// 13.27.e) - 13.27.5
void enableForwarding	(STP_BRIDGE*, int givenPort, int givenTree, unsigned int timestamp);	// 13.27.f) - 13.27.6
void enableLearning		(STP_BRIDGE*, int givenPort, int givenTree, unsigned int timestamp);	// 13.27.g) - 13.27.7
bool fromSameRegion		(STP_BRIDGE*, int givenPort);					// 13.27.h) - 13.27.8
void newTcDetected		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.i) - 13.27.9
void newTcWhile			(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.j) - 13.27.10
void pseudoRcvMsgs		(STP_BRIDGE*, int givenPort);					// 13.27.k) - 13.27.11
RCVD_INFO rcvInfo		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.l) - 13.27.12
void rcvMsgs			(STP_BRIDGE*, int givenPort);					// 13.27.m) - 13.27.13
void recordAgreement	(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.n) - 13.27.14
void recordDispute		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.o) - 13.27.15
void recordMastered		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.p) - 13.27.16
void recordPriority		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.q) - 13.27.17
void recordProposal		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.r) - 13.27.18
void recordTimes		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.s) - 13.27.19
void setReRootTree		(STP_BRIDGE*, int givenTree);					// 13.27.t) - 13.27.20
void setSelectedTree	(STP_BRIDGE*, int givenTree);					// 13.27.u) - 13.27.21
void setSyncTree		(STP_BRIDGE*, int givenTree);					// 13.27.v) - 13.27.22
void setTcFlags			(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.w) - 13.27.23
void setTcPropTree		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.x) - 13.27.24
void syncMaster			(STP_BRIDGE*);									// 13.27.y) - 13.27.25
void txConfig			(STP_BRIDGE*, int givenPort, unsigned int timestamp);					// 13.27.z) - 13.27.26
void txRstp				(STP_BRIDGE*, int givenPort, unsigned int timestamp);					// 13.27.aa) - 13.27.27
void txTcn				(STP_BRIDGE*, int givenPort, unsigned int timestamp);					// 13.27.ab) - 13.27.28
void updtBPDUVersion	(STP_BRIDGE*, int givenPort);					// 13.27.ac) - 13.27.29
void updtRcvdInfoWhile	(STP_BRIDGE*, int givenPort, int givenTree);	// 13.27.ad) - 13.27.30
void updtRolesTree		(STP_BRIDGE*, int givenTree);					// 13.27.ae) - 13.27.31
void updtRolesDisabledTree (STP_BRIDGE*, int givenTree);				// 13.27.af) - 13.27.32

// 13.26 State machine conditions and parameters
bool allSynced			(STP_BRIDGE*, int givenPort, int givenTree);	// 13.26.a) - 13.26.1
bool allTransmitReady	(STP_BRIDGE*, int givenPort);		// 13.26.b) - 13.26.2
bool cist				(STP_BRIDGE*, int givenTree);		// 13.26.c) - 13.26.3
bool cistRootPort		(STP_BRIDGE*, int givenPort);		// 13.26.d) - 13.26.4
bool cistDesignatedPort	(STP_BRIDGE*, int givenPort);		// 13.26.e) - 13.26.5
unsigned short EdgeDelay(STP_BRIDGE*, int givenPort);		// 13.26.f) - 13.26.6
unsigned short forwardDelay (STP_BRIDGE*, int givenPort);	// 13.26.g) - 13.26.7
unsigned short FwdDelay (STP_BRIDGE*, int givenPort);		// 13.26.h) - 13.26.8
unsigned short HelloTime(STP_BRIDGE*, int givenPort);		// 13.26.i) - 13.26.9
unsigned short MaxAge	(STP_BRIDGE*, int givenPort);		// 13.26.j) - 13.26.11
bool msti				(STP_BRIDGE*, int givenTree);		// 13.26.k) - 13.26.10
bool mstiDesignatedOrTCpropagatingRootPort (STP_BRIDGE*, int givenPort); // 13.26.l) - 13.26.12
bool mstiMasterPort		(STP_BRIDGE*, int givenPort);		// 13.26.m) - 13.26.13
bool rcvdAnyMsg			(STP_BRIDGE*, int givenPort);		// 13.26.o) - 13.26.15
bool rcvdCistMsg		(STP_BRIDGE*, int givenPort);		// 13.26.p) - 13.26.16
bool rcvdMstiMsg		(STP_BRIDGE*, int givenPort, int givenTree);	// 13.26.q) - 13.26.17
bool reRooted			(STP_BRIDGE*, int givenPort, int givenTree);	// 13.26.r) - 13.26.18
bool rstpVersion		(STP_BRIDGE*);						// 13.26.s) - 13.26.19
bool stpVersion			(STP_BRIDGE*);						// 13.26.t) - 13.26.20
bool updtCistInfo		(STP_BRIDGE*, int givenPort);		// 13.26.u) - 13.26.21
bool updtMstiInfo		(STP_BRIDGE*, int givenPort, int givenTree); // 13.26.v) - 13.26.22
bool rcvdXstMsg			(STP_BRIDGE*, int givenPort, int givenTree); // not in the standard
bool updtXstInfo		(STP_BRIDGE*, int givenPort, int givenTree); // not in the standard

// Not from the standard. See long comment in 802_1Q_2011_procedures.cpp, just above CallTcCallback().
void CallTcCallback (STP_BRIDGE* bridge);
void CallNotifiedTcCallback (STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp);

#endif
