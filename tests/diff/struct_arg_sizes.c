// B1: structs of every size class passed by value, each alone.
struct s3  { char a, b, c; };
struct s4  { int a; };
struct s6  { short a, b, c; };
struct s8  { int a, b; };
struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s20 { int a, b, c, d, e; };
struct s24 { long a, b, c; };
struct s40 { long a, b, c, d, e; };

long f3(struct s3 s){ return s.a + 2 * s.b + 3 * s.c; }
long f4(struct s4 s){ return s.a; }
long f6(struct s6 s){ return s.a + 2 * s.b + 3 * s.c; }
long f8(struct s8 s){ return s.a + 2 * s.b; }
long f12(struct s12 s){ return s.a + 2 * s.b + 3 * s.c; }
long f16(struct s16 s){ return s.a + 2 * s.b; }
long f20(struct s20 s){ return s.a + 2 * s.b + 3 * s.c + 4 * s.d + 5 * s.e; }
long f24(struct s24 s){ return s.a + 2 * s.b + 3 * s.c; }
long f40(struct s40 s){ return s.a + 2 * s.b + 3 * s.c + 4 * s.d + 5 * s.e; }

int main(void){
    struct s3 a = { 1, 2, 3 };
    struct s4 b = { 4 };
    struct s6 c = { 5, 6, 7 };
    struct s8 d = { 8, 9 };
    struct s12 e = { 10, 11, 12 };
    struct s16 f = { 13, 14 };
    struct s20 g = { 15, 16, 17, 18, 19 };
    struct s24 h = { 20, 21, 22 };
    struct s40 i = { 23, 24, 25, 26, 27 };
    long sum = f3(a) + f4(b) * 3 + f6(c) * 5 + f8(d) * 7 + f12(e) * 11 + f16(f) * 13
        + f20(g) * 17 + f24(h) * 19 + f40(i) * 23;
    return (int)(sum % 127);
}
