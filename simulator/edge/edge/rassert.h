
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once

extern volatile unsigned int assert_function_running;

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
extern __declspec(noinline) void __stdcall rassert_function (const char* expression, const char* file, unsigned line);
#define rassert(_expression)       (void)( (!!(_expression))   || (::IsDebuggerPresent() ? __debugbreak() : rassert_function(#_expression, __FILE__, __LINE__), 0) )

#ifndef NDEBUG
#define dassert(_expression)       rassert(_expression)
#else
#define dassert(_expression)       ((void)0)
#endif
