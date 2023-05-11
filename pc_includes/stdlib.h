#ifndef STDLIB_H
#define STDLIB_H
// size_t is pointer-sized: 4 bytes on x86, 8 on the LP64 targets.
// Two blocks instead of #else,
#ifndef __LP64__
typedef int size_t;
#endif
#ifdef __LP64__
typedef long size_t;
#endif

#endif
