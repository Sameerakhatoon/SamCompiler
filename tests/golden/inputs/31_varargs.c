// EXPECTED EXIT: 30
#include <stdarg.h>

int sum2(int num, ...)
{
    va_list list;
    va_start(list, num);
    int a = va_arg(list, int);
    int b = va_arg(list, int);
    return a + b;
}

int main()
{
    return sum2(2, 10, 20);
}
