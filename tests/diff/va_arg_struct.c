// va_arg of struct types: every size class (one word, two words, and
// the bigger ones x86_64 copies onto the stack and aarch64/riscv64 pass
// by reference), with scalars between them, and a va_list handed to
// another function.
#include <stdarg.h>

struct s4  { int a; };
struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s24 { long a, b, c; };
struct s40 { long a, b, c, d, e; };

long structs(int n, ...){
    va_list ap;
    va_start(ap, n);
    struct s4 a = va_arg(ap, struct s4);
    struct s12 b = va_arg(ap, struct s12);
    int k = va_arg(ap, int);
    struct s16 c = va_arg(ap, struct s16);
    struct s24 d = va_arg(ap, struct s24);
    struct s40 e = va_arg(ap, struct s40);
    long last = va_arg(ap, long);
    va_end(ap);
    return n + a.a + 2 * b.a + 3 * b.b + 5 * b.c + 7 * k + 11 * c.a + 13 * c.b
         + 17 * d.a + 19 * d.c + 23 * e.a + 29 * e.e + 31 * last;
}

long sum_points(int n, va_list ap){
    long total = 0;
    while(n-- > 0){
        struct s12 p = va_arg(ap, struct s12);
        total = total * 3 + p.a + p.b * p.c;
    }
    return total;
}

long points(int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = sum_points(n, ap);
    va_end(ap);
    return total;
}

int member(int n, ...){
    va_list ap;
    va_start(ap, n);
    int x = va_arg(ap, struct s24).b + n;
    va_end(ap);
    return x;
}

int main(void){
    struct s4 a = { 1 };
    struct s12 b = { 2, 3, 4 };
    struct s16 c = { 5, 6 };
    struct s24 d = { 7, 8, 9 };
    struct s40 e = { 10, 11, 12, 13, 14 };
    long s = structs(1, a, b, 6, c, d, e, 15L);
    if(s != 1 + 1 + 4 + 9 + 20 + 42 + 55 + 78 + 119 + 171 + 230 + 406 + 465) return 1;
    struct s12 p = { 1, 2, 3 };
    struct s12 q = { 4, 5, 6 };
    if(points(3, p, q, b) != ((7 * 3) + 34) * 3 + 14) return 2;
    if(member(2, d) != 10) return 3;
    return 0;
}
