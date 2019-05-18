
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2019 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_PROCEDURES_H
#define MSTP_LIB_PROCEDURES_H

#include "stp_base_types.h"

bool betterorsameInfo      (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, INFO_IS newInfoIs);
void clearAllRcvdMsgs      (STP_BRIDGE*, PortIndex portIndex);
void clearReselectTree     (STP_BRIDGE*, TreeIndex treeIndex);
void disableForwarding     (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);
void disableLearning       (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);
void enableForwarding      (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);
void enableLearning        (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);
bool fromSameRegion        (STP_BRIDGE*, PortIndex portIndex);
void newTcDetected         (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void newTcWhile            (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex, unsigned int timestamp);
void pseudoRcvMsgs         (STP_BRIDGE*, PortIndex portIndex);
RCVD_INFO rcvInfo          (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void rcvMsgs               (STP_BRIDGE*, PortIndex portIndex);
void rcvAgreements         (STP_BRIDGE*, PortIndex portIndex);
void recordAgreement       (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void recordDispute         (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void recordMastered        (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void recordPriority        (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void recordProposal        (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void recordTimes           (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void setReRootTree         (STP_BRIDGE*, TreeIndex treeIndex);
void setSelectedTree       (STP_BRIDGE*, TreeIndex treeIndex);
void setSyncTree           (STP_BRIDGE*, TreeIndex treeIndex);
void setTcFlags            (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void setTcPropTree         (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void syncMaster            (STP_BRIDGE*);
void txConfig              (STP_BRIDGE*, PortIndex portIndex, unsigned int timestamp);
void txRstp                (STP_BRIDGE*, PortIndex portIndex, unsigned int timestamp);
void txTcn                 (STP_BRIDGE*, PortIndex portIndex, unsigned int timestamp);
void updtBPDUVersion       (STP_BRIDGE*, PortIndex portIndex);
void updtRcvdInfoWhile     (STP_BRIDGE*, PortIndex portIndex, TreeIndex treeIndex);
void updtRolesTree         (STP_BRIDGE*, TreeIndex treeIndex);
void updtRolesDisabledTree (STP_BRIDGE*, TreeIndex treeIndex);

void CallNotifiedTcCallback (STP_BRIDGE* bridge, TreeIndex, unsigned int timestamp);

#endif
