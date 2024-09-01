/* gcc side of the 64-bit struct ABI interop test. */
struct s4  { int a; };
struct s8  { int a, b; };
struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s24 { long a, b, c; };
struct s40 { long a, b, c, d, e; };

/* Implemented by SamCompiler. */
struct s4  sam_ret4(int x);
struct s8  sam_ret8(int x);
struct s12 sam_ret12(int x);
struct s16 sam_ret16(long x);
struct s24 sam_ret24(long x);
long sam_take(struct s4 a, struct s8 b, struct s12 c, struct s16 d, struct s24 e, struct s40 f);
long sam_take_late(long r1, long r2, long r3, long r4, long r5, struct s16 s, long r6, struct s12 t);
long sam_calls_gcc(void);

/* Called by SamCompiler. */
struct s4  gcc_ret4(int x)   { struct s4 r = { x }; return r; }
struct s8  gcc_ret8(int x)   { struct s8 r = { x, x + 1 }; return r; }
struct s12 gcc_ret12(int x)  { struct s12 r = { x, x + 1, x + 2 }; return r; }
struct s16 gcc_ret16(long x) { struct s16 r = { x, x + 1 }; return r; }
struct s24 gcc_ret24(long x) { struct s24 r = { x, x + 1, x + 2 }; return r; }
long gcc_take(struct s4 a, struct s8 b, struct s12 c, struct s16 d, struct s24 e, struct s40 f){
    return a.a + b.a + b.b + c.a + c.b + c.c + d.a + d.b + e.a + e.b + e.c
        + f.a + f.b + f.c + f.d + f.e;
}
long gcc_take_late(long r1, long r2, long r3, long r4, long r5, struct s16 s, long r6, struct s12 t){
    return r1 + r2 + r3 + r4 + r5 + s.a * 10 + s.b * 100 + r6 * 1000 + t.a + t.b + t.c;
}

int main(void){
    int fail = 0;
    struct s4 a = sam_ret4(1);
    struct s8 b = sam_ret8(2);
    struct s12 c = sam_ret12(3);
    struct s16 d = sam_ret16(4);
    struct s24 e = sam_ret24(5);
    struct s40 f = { 6, 7, 8, 9, 10 };
    if(a.a != 1) fail |= 1;
    if(b.a != 2 || b.b != 3) fail |= 2;
    if(c.a != 3 || c.c != 5) fail |= 4;
    if(d.a != 4 || d.b != 5) fail |= 8;
    if(e.a != 5 || e.c != 7) fail |= 16;
    if(sam_take(a, b, c, d, e, f) != gcc_take(a, b, c, d, e, f)) fail |= 32;
    if(sam_take_late(1, 2, 3, 4, 5, d, 6, c) != gcc_take_late(1, 2, 3, 4, 5, d, 6, c)) fail |= 64;
    if(sam_calls_gcc() != 0) fail |= 128;
    return fail;
}
