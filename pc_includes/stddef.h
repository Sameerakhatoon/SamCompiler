#ifndef STDDEF_H
#define STDDEF_H

#include "stddef-internal.h"

#define offsetof(TYPE, MEMBER) &((TYPE*)0x00)->MEMBER

// size_t and NULL, which freestanding code (the SamOS stdlib
// headers) takes from here. size_t as in stdlib.h; two blocks, not
// #else.
#ifndef SAMCC_SIZE_T
#define SAMCC_SIZE_T
#ifndef __LP64__
typedef int size_t;
#endif
#ifdef __LP64__
typedef long size_t;
#endif
#endif

// ptrdiff_t, pointer-sized and signed.
#ifndef __LP64__
typedef int ptrdiff_t;
#endif
#ifdef __LP64__
typedef long ptrdiff_t;
#endif

// C11 max_align_t, aligned like the most aligned scalar SamCompiler has
// (long double is not supported). helpers/arena.c uses it.
#ifndef SAMCC_MAX_ALIGN_T
#define SAMCC_MAX_ALIGN_T
typedef struct { long __sam_l; double __sam_d; } max_align_t;
#endif

#define NULL 0

// The type of L"" and L'x': an int, unsigned on aarch64.
#ifndef SAMCC_WCHAR_T
#define SAMCC_WCHAR_T
#ifndef __aarch64__
typedef int wchar_t;
#endif
#ifdef __aarch64__
typedef unsigned int wchar_t;
#endif
#endif
#endif
