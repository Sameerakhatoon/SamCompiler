// B1: a struct parameter is the callee's own copy: changing it leaves
// the caller's struct alone, whether it came in registers, on the
// stack or by reference to a copy the caller made.
struct s8  { int a, b; };
struct s16 { long a, b; };
struct s40 { long a, b, c, d, e; };

long bump8(struct s8 s){ s.a = s.a + 100; s.b = 0; return s.a + s.b; }
long bump16(long r1, long r2, long r3, long r4, long r5, long r6, long r7, struct s16 s){
    s.a = s.a * 10;
    s.b = s.b * 10;
    return s.a + s.b + r1 + r7;
}
long bump40(struct s40 s){ s.a = 1000; s.e = 2000; return s.a + s.e + s.c; }
long bump40_late(long r1, long r2, long r3, long r4, long r5, long r6, long r7, long r8, struct s40 s){
    s.c = 99;
    return s.c + r8;
}

int main(void){
    struct s8 x = { 1, 2 };
    struct s16 y = { 3, 4 };
    struct s40 z = { 5, 6, 7, 8, 9 };
    long got = bump8(x) + bump16(1, 2, 3, 4, 5, 6, 7, y) + bump40(z) + bump40_late(1, 2, 3, 4, 5, 6, 7, 8, z);
    int ok = 0;
    if(x.a == 1 && x.b == 2) ok = ok + 1;
    if(y.a == 3 && y.b == 4) ok = ok + 2;
    if(z.a == 5 && z.c == 7 && z.e == 9) ok = ok + 4;
    return (int)(ok * 16 + got % 16);
}
