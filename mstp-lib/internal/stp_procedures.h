
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_PROCEDURES_H
#define MSTP_LIB_PROCEDURES_H

#include "stp_base_types.h"

// 13.27 State machine procedures
bool betterorsameInfo	(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, INFO_IS newInfoIs); // 13.27.a) - 13.27.1
void clearAllRcvdMsgs	(STP_BRIDGE*, PortIndex portIndex); // 13.27.b) - 13.27.2
void clearReselectTree	(STP_BRIDGE*, TreeIndex treeIndex); // 13.27.c) - 13.27.3
void disableForwarding	(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);	// 13.27.d) - 13.27.4
void disableLearning	(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);	// 13.27.e) - 13.27.5
void enableForwarding	(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);	// 13.27.f) - 13.27.6
void enableLearning		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);	// 13.27.g) - 13.27.7
bool fromSameRegion		(STP_BRIDGE*, PortIndex portIndex);					// 13.27.h) - 13.27.8
void newTcDetected		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.i) - 13.27.9
void newTcWhile			(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);	// 13.27.j) - 13.27.10
void pseudoRcvMsgs		(STP_BRIDGE*, PortIndex portIndex);					// 13.27.k) - 13.27.11
RCVD_INFO rcvInfo		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.l) - 13.27.12
void rcvMsgs			(STP_BRIDGE*, PortIndex portIndex);					// 13.27.m) - 13.27.13
void recordAgreement	(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.n) - 13.27.14
void recordDispute		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.o) - 13.27.15
void recordMastered		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.p) - 13.27.16
void recordPriority		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.q) - 13.27.17
void recordProposal		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.r) - 13.27.18
void recordTimes		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.s) - 13.27.19
void setReRootTree		(STP_BRIDGE*, TreeIndex treeIndex);					// 13.27.t) - 13.27.20
void setSelectedTree	(STP_BRIDGE*, TreeIndex treeIndex);					// 13.27.u) - 13.27.21
void setSyncTree		(STP_BRIDGE*, TreeIndex treeIndex);					// 13.27.v) - 13.27.22
void setTcFlags			(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.w) - 13.27.23
void setTcPropTree		(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.x) - 13.27.24
void syncMaster			(STP_BRIDGE*);									// 13.27.y) - 13.27.25
void txConfig			(STP_BRIDGE*, PortIndex portIndex, unsigned int timestamp);					// 13.27.z) - 13.27.26
void txRstp				(STP_BRIDGE*, PortIndex portIndex, unsigned int timestamp);					// 13.27.aa) - 13.27.27
void txTcn				(STP_BRIDGE*, PortIndex portIndex, unsigned int timestamp);					// 13.27.ab) - 13.27.28
void updtBPDUVersion	(STP_BRIDGE*, PortIndex portIndex);					// 13.27.ac) - 13.27.29
void updtRcvdInfoWhile	(STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);	// 13.27.ad) - 13.27.30
void updtRolesTree		(STP_BRIDGE*, TreeIndex treeIndex);					// 13.27.ae) - 13.27.31
void updtRolesDisabledTree (STP_BRIDGE*, TreeIndex treeIndex);				// 13.27.af) - 13.27.32

// Not from the standard. See long comment in 802_1Q_2011_procedures.cpp, just above CallTcCallback().
void CallNotifiedTcCallback (STP_BRIDGE* bridge, TreeIndex, unsigned int timestamp);

#endif
