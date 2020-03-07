
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_PROCEDURES_H
#define MSTP_LIB_PROCEDURES_H

#include "stp_base_types.h"

bool betterorsameInfo      (STP_BRIDGE*, PortIndex, TreeIndex, INFO_IS newInfoIs);
void clearAllRcvdMsgs      (STP_BRIDGE*, PortIndex);
void clearReselectTree     (STP_BRIDGE*, TreeIndex);
void disableForwarding     (STP_BRIDGE*, PortIndex, TreeIndex, unsigned int timestamp);
void disableLearning       (STP_BRIDGE*, PortIndex, TreeIndex, unsigned int timestamp);
void enableForwarding      (STP_BRIDGE*, PortIndex, TreeIndex, unsigned int timestamp);
void enableLearning        (STP_BRIDGE*, PortIndex, TreeIndex, unsigned int timestamp);
bool fromSameRegion        (STP_BRIDGE*, PortIndex);
void newTcDetected         (STP_BRIDGE*, PortIndex, TreeIndex);
void newTcWhile            (STP_BRIDGE*, PortIndex, TreeIndex, unsigned int timestamp);
void pseudoRcvMsgs         (STP_BRIDGE*, PortIndex);
RCVD_INFO rcvInfo          (STP_BRIDGE*, PortIndex, TreeIndex);
void rcvMsgs               (STP_BRIDGE*, PortIndex);
void rcvAgreements         (STP_BRIDGE*, PortIndex);
void recordAgreement       (STP_BRIDGE*, PortIndex, TreeIndex);
void recordDispute         (STP_BRIDGE*, PortIndex, TreeIndex);
void recordMastered        (STP_BRIDGE*, PortIndex, TreeIndex);
void recordPriority        (STP_BRIDGE*, PortIndex, TreeIndex);
void recordProposal        (STP_BRIDGE*, PortIndex, TreeIndex);
void recordTimes           (STP_BRIDGE*, PortIndex, TreeIndex);
void setReRootTree         (STP_BRIDGE*, TreeIndex);
void setSelectedTree       (STP_BRIDGE*, TreeIndex);
void setSyncTree           (STP_BRIDGE*, TreeIndex);
void setTcFlags            (STP_BRIDGE*, PortIndex, TreeIndex);
void setTcPropTree         (STP_BRIDGE*, PortIndex, TreeIndex);
void syncMaster            (STP_BRIDGE*);
void txConfig              (STP_BRIDGE*, PortIndex, unsigned int timestamp);
void txRstp                (STP_BRIDGE*, PortIndex, unsigned int timestamp);
void txTcn                 (STP_BRIDGE*, PortIndex, unsigned int timestamp);
void updtAgreement         (STP_BRIDGE*, PortIndex, TreeIndex);
void updtBPDUVersion       (STP_BRIDGE*, PortIndex);
void updtDigest            (STP_BRIDGE*, PortIndex);
void updtRcvdInfoWhile     (STP_BRIDGE*, PortIndex, TreeIndex);
void updtRolesTree         (STP_BRIDGE*, TreeIndex);
void updtRolesDisabledTree (STP_BRIDGE*, TreeIndex);

#endif
