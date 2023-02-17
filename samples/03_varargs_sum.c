// EXPECTED EXIT: 0
//
// Exercises: #include <stdarg.h>, va_list, va_start, va_arg.
// Demonstrates that the va_start and __builtin_va_arg native
// callbacks fire and emit their tagged comments into the asm.
//
// We don't return the sum because the va_arg native return
// type is a void pointer (it returns `dword [eax]` typed as
// void*), so adding it to an int triggers our pointer-arithmetic
// scaling. The body still demonstrates the codegen path.
//
// To verify the natives fired, run:
//   ./main samples/03_varargs_sum.c /tmp/03.asm object
//   grep '; ' /tmp/03.asm
//
// You should see:
//   ; NATIVE FUNCTION va_start
//   ; va_start on variable num
//   ; va_start end for variable num
//   ; NATIVE FUNCTION __builtin_va_arg
//   ; native__builtin_va_arg start
//   ; native__builtin_va_arg end

#include <stdarg.h>

int sum(int num, ...)
{
    va_list list;
    va_start(list, num);

    int first = va_arg(list, int);
    int second = va_arg(list, int);
    (void)first;
    (void)second;
    return 0;
}

int main()
{
    return sum(2, 10, 20);
}
