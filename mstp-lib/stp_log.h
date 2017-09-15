
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_LOG_H
#define MSTP_LIB_LOG_H

struct STP_BRIDGE;

void STP_Log (STP_BRIDGE* bridge, int port, int tree, const char* format, ...);
void STP_FlushLog (STP_BRIDGE* bridge);
void STP_Indent (STP_BRIDGE* bridge);
void STP_Unindent (STP_BRIDGE* bridge);

#define LOG(b,p,t,...)		((void) ( !(b)->loggingEnabled || (STP_Log(b,p,t,__VA_ARGS__), 0)))
#define FLUSH_LOG(b)		((void) ( !(b)->loggingEnabled || (STP_FlushLog(b), 0)))
#define LOG_INDENT(b)		((void) ( !(b)->loggingEnabled || (STP_Indent(b), 0)))
#define LOG_UNINDENT(b)		((void) ( !(b)->loggingEnabled || (STP_Unindent(b), 0)))

#endif
