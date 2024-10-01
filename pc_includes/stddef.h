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

#define NULL 0
#endif
