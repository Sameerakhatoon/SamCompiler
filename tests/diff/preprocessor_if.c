// #if and #elif conditions as glibc's headers write them:
// `defined` with and without parentheses, joined by && and || with no
// parentheses around each, unsigned and negative values, ?:, shifts, a
// function-like macro in the condition, short-circuit evaluation, and
// a condition continued with backslashes, with comments in it. (A
// `defined` that a macro gives is undefined behaviour in C and not
// tested.)
#define _STDLIB_H 1
#define ZERO 0
#define PREREQ(maj, min) ((MAJOR << 16) + MINOR >= ((maj) << 16) + (min))
#define MAJOR 4
#define MINOR 2
#define USE(F) USE_ ## F
#define USE_FEATURE 1

int checks = 0;

#if !defined _SYS_WAIT_H && !defined _STDLIB_H
#error wrong 1
#else
int c1 = 1;
#endif

#if defined _STDLIB_H && !defined _SYS_WAIT_H
int c2 = 1;
#else
#error wrong 2
#endif

#if !defined(_SYS_WAIT_H) && defined(_STDLIB_H) || defined NOT_THERE
int c3 = 1;
#endif

#if -1
int c4 = 1;
#else
#error wrong 4
#endif

#if -1 > 0u
int c5 = 1;
#else
#error wrong 5 (unsigned compare)
#endif

#if (1 ? 2 : 3) == 2 && (0 ? 1 / ZERO : 7) == 7 && (1 << 4) == 16 && (-16 >> 2) == -4
int c6 = 1;
#else
#error wrong 6
#endif

#if PREREQ (4, 1) && !PREREQ (4, 3) && PREREQ(3, 99)
int c7 = 1;
#else
#error wrong 7
#endif

#if USE (FEATURE) && !USE (OTHER)
int c8 = 1;
#else
#error wrong 8
#endif

#if ZERO && 1 / ZERO
#error wrong 9
#elif 0 || 1 % 2 == 1
int c9 = 1;
#endif

#if NOT_A_MACRO == 0 && NOT_A_MACRO + 1 == 1
int c10 = 1;
#endif

#ifdef __has_include
#if __has_include(<stdio.h>)
int has_stdio;
#endif
#endif

#if 0x7fffffffffffffff > 0 && 18446744073709551615u == -1 && 'a' == 97
int c11 = 1;
#else
#error wrong 11
#endif

#define IS_ON(x) ((x) + 0 != 0)
#if IS_ON(USE_FEATURE) && !IS_ON(ZERO)
int c12 = 1;
#else
#error wrong 12
#endif

#if (defined _STDLIB_H /* a comment */ || defined NOT_THERE) /* and one \
    more */ && !defined _SYS_WAIT_H /* before */ \
    && 1
int c13 = 1;
#else
#error wrong 13
#endif

int main(void){
    return c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8 + c9 + c10 + c11 + c12 + c13 == 13 ? 0 : 1;
}
