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

// The limits of those types.
#define INT8_MIN    (-128)
#define INT8_MAX    127
#define UINT8_MAX   255
#define INT16_MIN   (-32768)
#define INT16_MAX   32767
#define UINT16_MAX  65535
#define INT32_MIN   (-2147483647 - 1)
#define INT32_MAX   2147483647
#define UINT32_MAX  4294967295U
#ifdef __LP64__
#define INT64_MIN   (-9223372036854775807L - 1)
#define INT64_MAX   9223372036854775807L
#define UINT64_MAX  18446744073709551615UL
#define INTPTR_MIN  INT64_MIN
#define INTPTR_MAX  INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#define SIZE_MAX    UINT64_MAX
#endif
#ifndef __LP64__
#define INTPTR_MIN  INT32_MIN
#define INTPTR_MAX  INT32_MAX
#define UINTPTR_MAX UINT32_MAX
#define SIZE_MAX    UINT32_MAX
#endif
#endif
