// SamCompiler side of the 64-bit struct ABI interop test
// (tests/283-abi64-interop.sh); gcc.c is the gcc side.
struct s4  { int a; };
struct s8  { int a; int b; };
struct s12 { int a; int b; int c; };
struct s16 { long a; long b; };
struct s24 { long a; long b; long c; };
struct s40 { long a; long b; long c; long d; long e; };

struct s4  gcc_ret4(int x);
struct s8  gcc_ret8(int x);
struct s12 gcc_ret12(int x);
struct s16 gcc_ret16(long x);
struct s24 gcc_ret24(long x);
long gcc_take(struct s4 a, struct s8 b, struct s12 c, struct s16 d, struct s24 e, struct s40 f);
long gcc_take_late(long r1, long r2, long r3, long r4, long r5, struct s16 s, long r6, struct s12 t);

struct s4 sam_ret4(int x){ struct s4 r; r.a = x; return r; }
struct s8 sam_ret8(int x){ struct s8 r; r.a = x; r.b = x + 1; return r; }
struct s12 sam_ret12(int x){ struct s12 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
struct s16 sam_ret16(long x){ struct s16 r; r.a = x; r.b = x + 1; return r; }
struct s24 sam_ret24(long x){ struct s24 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }

long sam_take(struct s4 a, struct s8 b, struct s12 c, struct s16 d, struct s24 e, struct s40 f){
    return a.a + b.a + b.b + c.a + c.b + c.c + d.a + d.b + e.a + e.b + e.c
        + f.a + f.b + f.c + f.d + f.e;
}
long sam_take_late(long r1, long r2, long r3, long r4, long r5, struct s16 s, long r6, struct s12 t){
    return r1 + r2 + r3 + r4 + r5 + s.a * 10 + s.b * 100 + r6 * 1000 + t.a + t.b + t.c;
}

long sam_calls_gcc(void){
    long fail = 0;
    struct s4 a = gcc_ret4(1);
    struct s8 b = gcc_ret8(2);
    struct s12 c = gcc_ret12(3);
    struct s16 d = gcc_ret16(4);
    struct s24 e = gcc_ret24(5);
    struct s40 f;
    f.a = 6; f.b = 7; f.c = 8; f.d = 9; f.e = 10;
    if(a.a != 1) fail = fail | 1;
    if(b.a != 2 || b.b != 3) fail = fail | 2;
    if(c.a != 3 || c.c != 5) fail = fail | 4;
    if(d.a != 4 || d.b != 5) fail = fail | 8;
    if(e.a != 5 || e.c != 7) fail = fail | 16;
    if(gcc_take(a, b, c, d, e, f) != 1 + 5 + 12 + 9 + 18 + 40) fail = fail | 32;
    if(gcc_take_late(1, 2, 3, 4, 5, d, 6, c) != 15 + 40 + 500 + 6000 + 12) fail = fail | 64;
    return fail;
}
