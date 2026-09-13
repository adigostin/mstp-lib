
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once

#ifdef __midl
	#define guidPropertyGroup        2884893D-F98C-4FEC-8D8A-D1FB4D852BAC
#else
	DEFINE_GUID(guidPropertyGroup, 0x2884893D, 0xF98C, 0x4FEC, 0x8D, 0x8A, 0xD1, 0xFB, 0x4D, 0x85, 0x2B, 0xAC);
#endif

#ifdef __midl
	#define guidPropertyDefaultValue 5F36D1A2-08BD-428C-95FB-0DB61BEBC6B2
#else
	DEFINE_GUID(guidPropertyDefaultValue, 0x5F36D1A2, 0x08BD, 0x428C, 0x95, 0xFB, 0x0D, 0xB6, 0x1B, 0xEB, 0xC6, 0xB2);
#endif

#ifdef __midl
	#define guidPropertyCustomEditor DA6DF6A7-F855-4D25-95CE-A00527E5B5A6
#else
	DEFINE_GUID(guidPropertyCustomEditor, 0xDA6DF6A7, 0xF855, 0x4D25, 0x95, 0xCE, 0xA0, 0x05, 0x27, 0xE5, 0xB5, 0xA6);
#endif
