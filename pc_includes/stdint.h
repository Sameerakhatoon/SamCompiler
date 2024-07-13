#ifndef STDINT_H
#define STDINT_H
// the fixed-width types for freestanding code (the samos
// targets). The 64-bit ones are long on the LP64 targets; x86 has no
// 64-bit integer type.
typedef signed char    int8_t;
typedef unsigned char  uint8_t;
typedef short          int16_t;
typedef unsigned short uint16_t;
typedef int            int32_t;
typedef unsigned int   uint32_t;
#ifdef __LP64__
typedef long           int64_t;
typedef unsigned long  uint64_t;
typedef long           intptr_t;
typedef unsigned long  uintptr_t;
#endif
#ifndef __LP64__
typedef int            intptr_t;
typedef unsigned int   uintptr_t;
#endif
#endif
