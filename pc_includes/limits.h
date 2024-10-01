#ifndef LIMITS_H
#define LIMITS_H
// The integer limits of SamCompiler's types:
// ILP32 on x86, LP64 on the others; plain char is signed on x86 and
// x86_64 and unsigned on aarch64 and riscv64. Separate #ifdef and
// #ifndef blocks rather than #else.
#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define SHRT_MIN   (-32768)
#define SHRT_MAX   32767
#define USHRT_MAX  65535
#define INT_MIN    (-2147483647 - 1)
#define INT_MAX    2147483647
#define UINT_MAX   4294967295U

#ifdef __aarch64__
#define SAMCC_CHAR_UNSIGNED 1
#endif
#ifdef __riscv
#define SAMCC_CHAR_UNSIGNED 1
#endif
#ifdef SAMCC_CHAR_UNSIGNED
#define CHAR_MIN   0
#define CHAR_MAX   255
#endif
#ifndef SAMCC_CHAR_UNSIGNED
#define CHAR_MIN   (-128)
#define CHAR_MAX   127
#endif

#ifdef __LP64__
#define LONG_MIN   (-9223372036854775807L - 1)
#define LONG_MAX   9223372036854775807L
#define ULONG_MAX  18446744073709551615UL
#define LLONG_MIN  (-9223372036854775807L - 1)
#define LLONG_MAX  9223372036854775807L
#define ULLONG_MAX 18446744073709551615UL
#endif
#ifndef __LP64__
#define LONG_MIN   (-2147483647 - 1)
#define LONG_MAX   2147483647
#define ULONG_MAX  4294967295U
#endif
#endif
