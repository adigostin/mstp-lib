
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib 
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_HMAC_MD5_H
#define MSTP_LIB_HMAC_MD5_H

// Data structure for MD5 (Message Digest) computation
struct MD5_CTX
{
	unsigned int i [2];		// number of _bits_ handled mod 2^64
	unsigned int buf [4];	// scratch buffer
	unsigned char in [64];	// input buffer
};

struct HMAC_MD5_CONTEXT : MD5_CTX
{
	unsigned char digest[16];	// actual digest after HMAC_MD5_End call
};

void HMAC_MD5_Init (HMAC_MD5_CONTEXT* context);
void HMAC_MD5_Update (HMAC_MD5_CONTEXT* context, const void* text, unsigned int text_len);
void HMAC_MD5_End (HMAC_MD5_CONTEXT* context);

#endif
