
#pragma once

extern volatile unsigned int assert_function_running;

#ifdef _MSC_VER
	extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
	extern __declspec(noinline) void __stdcall assert_function (const char* expression, const char* file, unsigned line);
	#undef assert
	#define assert(_Expression)       (void)( (!!(_Expression))   || (::IsDebuggerPresent() ? __debugbreak() : assert_function(#_Expression, __FILE__, __LINE__), 0) )
#else
	#error
#endif
