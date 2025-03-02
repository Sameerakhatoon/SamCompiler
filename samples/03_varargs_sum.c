// EXPECTED EXIT: 60
//
// Exercises: #include <stdarg.h>, va_list, va_start, va_arg,
// va_end. Three integers summed = 10 + 20 + 30 = 60. The native
// va_start callback emits the "; va_start on variable num"
// asm comment; verify that lands in the emitted .asm.

#include <stdarg.h>

int sum(int num, ...)
{
    int result = 0;
    va_list list;
    va_start(list, num);
    int i = 0;
    for (i = 0; i < num; i = i + 1) {
        result = result + va_arg(list, int);
    }
    va_end(list);
    return result;
}

int main()
{
    return sum(3, 10, 20, 30);
}
