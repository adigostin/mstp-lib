
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once

extern volatile unsigned int assert_function_running;

#undef assert

#ifdef _MSC_VER
	extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
	extern __declspec(noinline) void __stdcall assert_function (const char* expression, const char* file, unsigned line);
	#define assert(_Expression)       (void)( (!!(_Expression))   || (::IsDebuggerPresent() ? __debugbreak() : assert_function(#_Expression, __FILE__, __LINE__), 0) )
#else
	#ifdef __cplusplus
	extern "C" {
	#endif

	extern void __assert(const char *__filename, int __line);

	#ifdef __cplusplus
	}
	#endif

	#ifdef NDEBUG
	#define assert(ignore) ((void)0)
	#else
	#define assert(e) ((e) ? (void)0 : __assert(__FILE__, __LINE__))
	#endif
#endif
