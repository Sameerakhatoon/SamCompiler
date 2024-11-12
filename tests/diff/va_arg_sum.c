// va_arg inside a larger expression (`total + va_arg(...)`), which on
// x86 added the va_list's address instead of the running total, and
// variadic functions with a struct parameter in front, the last named
// parameter wider than the arguments after it, and char and short
// values promoted to int.
#include <stdarg.h>

struct s8  { int a, b; };
struct s16 { long a, b; };

long vsum(int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = 0;
    for(int i = 0; i < n; i++){
        total = total + va_arg(ap, long);
    }
    va_end(ap);
    return total;
}

long vsum_struct(struct s16 s, int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = s.a + s.b;
    for(int i = 0; i < n; i++){
        total = total + va_arg(ap, long) * 2;
    }
    va_end(ap);
    return total;
}

int after_struct(int n, struct s8 last, ...){
    va_list ap;
    va_start(ap, last);
    int total = last.a * last.b;
    for(int i = 0; i < n; i++){
        total = total - va_arg(ap, int);
    }
    va_end(ap);
    return total;
}

int promoted(int n, ...){
    va_list ap;
    va_start(ap, n);
    int c = va_arg(ap, int);
    int s = va_arg(ap, int);
    unsigned u = va_arg(ap, unsigned);
    char* p = va_arg(ap, char*);
    va_end(ap);
    return c + s + (u > 100) + p[1];
}

int main(void){
    struct s16 s = { 3, 40 };
    struct s8 t = { 6, 7 };
    signed char c = -5;
    short h = 300;
    if(vsum(3, 5L, 6L, 7L) != 18) return 1;
    if(vsum_struct(s, 3, 5L, 6L, 7L) != 79) return 2;
    if(after_struct(2, t, 10, 20) != 12) return 3;
    if(promoted(4, c, h, 4000000000u, "xy") != -5 + 300 + 1 + 'y') return 4;
    return (int)(vsum(4, 1L, 2L, 3L, 4L) + vsum_struct(s, 1, 1L));
}
