
#pragma once

#undef assert

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
