// B1: struct arguments and results through function pointers, a struct
// a call returns passed straight on, and printf (variadic) called
// next to struct arguments.
#include <stdio.h>

struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s24 { long a, b, c; };

struct s12 make12(int x){ struct s12 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
struct s24 make24(long x){ struct s24 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
long take12(struct s12 s){ return s.a + 2 * s.b + 3 * s.c; }
long take24(struct s24 s){ return s.a + 2 * s.b + 3 * s.c; }
long both(struct s12 x, long k, struct s24 y){ return take12(x) * k + take24(y); }
struct s16 swap16(struct s16 s){ struct s16 r; r.a = s.b; r.b = s.a; return r; }

int main(void){
    long (*t12)(struct s12) = take12;
    struct s16 (*sw)(struct s16) = swap16;
    long (*b)(struct s12, long, struct s24) = both;
    struct s16 p = { 3, 40 };
    // One call per statement: the x86 backend (i386) mishandles a
    // struct a call returns inside a bigger expression.
    long sum = t12(make12(1));
    long v = take24(make24(2));
    sum = sum + v;
    v = take12(make12(take12(make12(0)) % 7));
    sum = sum + v;
    struct s12 x = make12(3);
    struct s24 y = make24(4);
    v = b(x, 2, y);
    sum = sum + v;
    struct s16 q = sw(p);
    struct s16 r = sw(q);
    sum = sum + q.a - q.b + r.a;
    struct s12 m = make12(5);
    printf("%d %ld %ld\n", m.c, q.a, sum);
    return (int)(sum % 127);
}
