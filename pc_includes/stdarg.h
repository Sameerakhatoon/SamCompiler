#ifndef STDARG_H
#define STDARG_H
#include <stdarg-internal.h>

// va_list holds an address into the argument area, so it is as wide
// as a pointer: 4 bytes on x86, 8 on the LP64 targets. Two separate
// blocks instead of #else: a typedef in a skipped #else branch trips
// the preprocessor.
#ifndef __LP64__
typedef int __builtin_va_list;
#endif
#ifdef __LP64__
typedef long __builtin_va_list;
#endif
typedef __builtin_va_list va_list;

#define va_arg(list, type) __builtin_va_arg(list, sizeof(type))
#endif
