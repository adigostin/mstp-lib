
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_CONDS_AND_PARAMS_H
#define MSTP_LIB_CONDS_AND_PARAMS_H

#include "stp_base_types.h"

// 13.28 State machine conditions and parameters
bool allSptAgree        (const STP_BRIDGE*);
bool allSynced			(const STP_BRIDGE*, PortIndex, TreeIndex);
bool allTransmitReady	(const STP_BRIDGE*, PortIndex);
const PRIORITY_VECTOR& BestAgreementPriority();
bool cist				(const STP_BRIDGE*, TreeIndex);
bool cistRootPort		(const STP_BRIDGE*, PortIndex);
bool cistDesignatedPort	(const STP_BRIDGE*, PortIndex);
unsigned short EdgeDelay    (const STP_BRIDGE*, PortIndex);
unsigned short forwardDelay (const STP_BRIDGE*, PortIndex);
unsigned short FwdDelay     (const STP_BRIDGE*, PortIndex);
unsigned short HelloTime    (const STP_BRIDGE*, PortIndex);
unsigned short MaxAge	    (const STP_BRIDGE*, PortIndex);
bool msti				(const STP_BRIDGE*, TreeIndex);
bool mstiDesignatedOrTCpropagatingRootPort (const STP_BRIDGE*, PortIndex);
bool mstiMasterPort		(const STP_BRIDGE*, PortIndex);
bool operPointToPoint   (const STP_BRIDGE*, PortIndex);
bool rcvdAnyMsg			(const STP_BRIDGE*, PortIndex);
bool rcvdCistMsg		(const STP_BRIDGE*, PortIndex);
bool rcvdMstiMsg		(const STP_BRIDGE*, PortIndex, TreeIndex);
bool reRooted			(const STP_BRIDGE*, PortIndex, TreeIndex);
bool rstpVersion		(const STP_BRIDGE*);
bool spt                (const STP_BRIDGE*);
bool stpVersion			(const STP_BRIDGE*);
bool updtCistInfo		(const STP_BRIDGE*, PortIndex);
bool updtMstiInfo		(const STP_BRIDGE*, PortIndex, TreeIndex);
bool rcvdXstMsg			(const STP_BRIDGE*, PortIndex, TreeIndex);
bool updtXstInfo		(const STP_BRIDGE*, PortIndex, TreeIndex);

#endif
