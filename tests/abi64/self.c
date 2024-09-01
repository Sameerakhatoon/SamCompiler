// B1: struct calls the x86 backend (i386) cannot build yet, so
// tests/diff, which runs every target, cannot hold them: a member of a
// struct a call returns inside an expression, a struct parameter
// returned whole, struct results passed straight on through a function
// pointer, and variadic callees with a struct parameter in front (on
// riscv64 one split between a7 and the stack).
#include <stdarg.h>

struct s3  { char a, b, c; };
struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s24 { long a, b, c; };
struct s40 { long a, b, c, d, e; };

struct s3 make3(int x){ struct s3 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
struct s12 make12(int x){ struct s12 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
struct s24 make24(long x){ struct s24 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
struct s12 pass12(struct s12 s){ return s; }
struct s40 pass40(struct s40 s){ s.c = s.c + 1; return s; }
long both(struct s12 x, long k, struct s24 y){ return (x.a + 2 * x.b + 3 * x.c) * k + y.a + 2 * y.b + 3 * y.c; }

long vsum(struct s16 s, int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = s.a + s.b;
    for(int i = 0; i < n; i++){
        total = total + va_arg(ap, long);
    }
    va_end(ap);
    return total;
}

long late(long r1, long r2, long r3, long r4, long r5, long r6, long r7, struct s16 s, int n, ...){
    va_list ap;
    va_start(ap, n);
    long total = r1 + r2 + r3 + r4 + r5 + r6 + r7 + 10 * s.a + 100 * s.b;
    for(int i = 0; i < n; i++){
        total = total + 7 * va_arg(ap, long);
    }
    va_end(ap);
    return total;
}

int main(void){
    long (*b)(struct s12, long, struct s24) = both;
    struct s16 p = { 3, 40 };
    struct s40 f = { 1, 2, 3, 4, 5 };
    long sum = make12(2).c + pass12(make12(3)).b * 2;
    sum = sum + pass40(f).c * 3 + f.c;
    sum = sum + b(make12(3), 2, make24(4));
    sum = sum + vsum(p, 3, 5L, 6L, 7L);
    sum = sum + late(1, 2, 3, 4, 5, 6, 7, p, 2, 1L, 2L);
    sum = sum + make3(7).c + both(make12(1), make3(2).b, make24(make12(1).a));
    return (int)(sum % 127);
}
