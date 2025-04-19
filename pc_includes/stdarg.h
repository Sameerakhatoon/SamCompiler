#ifndef STDARG_H
#define STDARG_H
#include <stdarg-internal.h>

// On x86 va_list is the address of the next argument on the stack, 4
// bytes. On aarch64 and riscv64 it is five addresses (exp64_stdarg):
// the next argument register's home slot, the end of those slots, the
// next stack argument, which aarch64 needs apart because a value that
// does not fit in the registers left goes to the stack while later
// arguments may still take registers, and the next floating-point
// argument register's home slot and the end of those slots, where
// aarch64 passes a variadic double.
// on x86_64 it is a pointer to the psABI's state,
// `struct __va_list_tag`, which va_start and va_copy keep in the frame
// of the function that calls them. glibc's vprintf family takes the
// psABI's va_list, an array of one such struct, as a pointer to it: the
// same thing. Two separate blocks instead of #else.
#ifndef __LP64__
typedef int __builtin_va_list;
#endif
#ifdef __x86_64__
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void* overflow_arg_area;
    void* reg_save_area;
} *__builtin_va_list;
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#endif
#ifndef __x86_64__
#ifdef __LP64__
typedef struct __sam_va_list { long __gp; long __gp_end; long __overflow; long __fp; long __fp_end; } __builtin_va_list;
#endif
#define va_copy(dest, src) (dest) = (src)
#endif
typedef __builtin_va_list va_list;
// The name glibc's headers declare their vprintf family with.
typedef __builtin_va_list __gnuc_va_list;
#define __va_list_defined

// __builtin_va_arg steps the list past one argument of that size and
// gives the argument's address, so every type, a struct too, is read
// at its own width. The third argument only carries the type, so that
// a double is taken from where floating-point arguments go.
#define va_arg(list, type) (*(type*)__builtin_va_arg(list, sizeof(type), (type*)0))
#endif
