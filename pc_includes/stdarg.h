#ifndef STDARG_H
#define STDARG_H
#include <stdarg-internal.h>

// On x86 va_list is the address of the next argument on the stack, 4
// bytes. On the LP64 targets it is five addresses (exp64_stdarg): the
// next argument register's home slot, the end of those slots, the next
// stack argument, which x86_64 and aarch64 need apart because a value
// that does not fit in the registers left goes to the stack while
// later arguments may still take registers, and the next
// floating-point argument register's home slot and the end of those
// slots, where x86_64 and aarch64 pass a variadic double. Two separate
// blocks instead of #else: a typedef in a skipped #else branch trips
// the preprocessor.
#ifndef __LP64__
typedef int __builtin_va_list;
#endif
#ifdef __LP64__
typedef struct __sam_va_list { long __gp; long __gp_end; long __overflow; long __fp; long __fp_end; } __builtin_va_list;
#endif
typedef __builtin_va_list va_list;

// __builtin_va_arg steps the list past one argument of that size and
// gives the argument's address, so every type, a struct too, is read
// at its own width. The third argument only carries the type, so that
// a double is taken from where floating-point arguments go.
#define va_arg(list, type) (*(type*)__builtin_va_arg(list, sizeof(type), (type*)0))
// No parentheses around the assignment: the x86 generator miscompiles
// a parenthesized assignment used as a statement.
#define va_copy(dest, src) (dest) = (src)
#endif
