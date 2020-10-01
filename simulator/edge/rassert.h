
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once

extern volatile unsigned int assert_function_running;

// TODO: class assertion_failed_exception

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
extern __declspec(noinline) void __stdcall rassert_function (const char* expression, const char* file, unsigned line);
#define rassert(_Expression)       (void)( (!!(_Expression))   || (::IsDebuggerPresent() ? __debugbreak() : rassert_function(#_Expression, __FILE__, __LINE__), 0) )
