// Variadic arguments where the argument registers run out. With five
// named arguments a 16-byte struct finds one register left: x86_64
// puts it on the stack and gives the register to the next long, and
// aarch64 and riscv64 take two registers. With seven named arguments
// x86_64 has every variadic argument on the stack, aarch64 puts the
// struct on the stack and uses no register after it, and riscv64
// splits it between a7 and the stack. Named structs that went to the
// stack while later named scalars took registers move the first
// variadic argument too.
#include <stdarg.h>

struct s16 { long a, b; };

long five(long a, long b, long c, long d, long e, ...){
    va_list ap;
    va_start(ap, e);
    struct s16 s = va_arg(ap, struct s16);
    long x = va_arg(ap, long);
    long y = va_arg(ap, long);
    va_end(ap);
    return a + b + c + d + e + 10 * s.a + 100 * s.b + 1000 * x + 10000 * y;
}

long seven(long a, long b, long c, long d, long e, long f, long g, ...){
    va_list ap;
    va_start(ap, g);
    struct s16 s = va_arg(ap, struct s16);
    long x = va_arg(ap, long);
    va_end(ap);
    return a + b + c + d + e + f + g + 10 * s.a + 100 * s.b + 1000 * x;
}

long named_struct(long a, long b, long c, long d, long e, struct s16 s, int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = a + b + c + d + e + s.a * 3 + s.b * 5;
    for(int i = 0; i < n; i++){
        total = total * 2 + va_arg(ap, long);
    }
    va_end(ap);
    return total;
}

long many(int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = 0;
    for(int i = 0; i < n; i++){
        total = total * 3 + va_arg(ap, int);
    }
    va_end(ap);
    return total;
}

int main(void){
    struct s16 s = { 2, 3 };
    if(five(1, 1, 1, 1, 1, s, 4L, 5L) != 5 + 20 + 300 + 4000 + 50000) return 1;
    if(seven(1, 1, 1, 1, 1, 1, 1, s, 4L) != 7 + 20 + 300 + 4000) return 2;
    if(named_struct(1, 1, 1, 1, 1, s, 3, 7L, 8L, 9L) != ((5 + 6 + 15) * 2 + 7) * 4 + 8 * 2 + 9) return 3;
    int v[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -12 };
    long m = 0;
    for(int i = 0; i < 12; i++){
        m = m * 3 + v[i];
    }
    if(many(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -12) != m) return 4;
    return 0;
}
