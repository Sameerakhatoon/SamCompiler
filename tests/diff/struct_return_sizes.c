// B1: structs returned by value: up to 16 bytes in two registers,
// bigger through the hidden pointer, with and without more than six
// or eight arguments next to it.
struct s3  { char a, b, c; };
struct s4  { int a; };
struct s8  { int a, b; };
struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s20 { int a, b, c, d, e; };
struct s24 { long a, b, c; };

struct s3 ret3(int x){ struct s3 r; r.a = x; r.b = x + 1; r.c = x + 2; return r; }
struct s4 ret4(int x){ struct s4 r; r.a = x; return r; }
struct s8 ret8(int x){ struct s8 r; r.a = x; r.b = x * 2; return r; }
struct s12 ret12(int x){ struct s12 r; r.a = x; r.b = x * 2; r.c = x * 3; return r; }
struct s16 ret16(long x){ struct s16 r; r.a = x; r.b = x * 2; return r; }
struct s20 ret20(int x){ struct s20 r = { x, x + 1, x + 2, x + 3, x + 4 }; return r; }
struct s24 ret24(long x){ struct s24 r; r.a = x; r.b = x * 2; r.c = x * 3; return r; }

struct s16 ret16_many(long a, long b, long c, long d, long e, long f, long g, long h, long i){
    struct s16 r;
    r.a = a + b + c + d + e;
    r.b = f + g + h + 2 * i;
    return r;
}

struct s24 ret24_many(long a, long b, long c, long d, long e, long f, long g, long h, long i){
    struct s24 r;
    r.a = a + b + c;
    r.b = d + e + f;
    r.c = g + h + 2 * i;
    return r;
}

int main(void){
    struct s3 a = ret3(5);
    struct s4 b = ret4(7);
    struct s8 c = ret8(9);
    struct s12 d = ret12(11);
    struct s16 e = ret16(13);
    struct s20 f = ret20(15);
    struct s24 g = ret24(17);
    struct s16 h = ret16_many(1, 2, 3, 4, 5, 6, 7, 8, 9);
    struct s24 i = ret24_many(1, 2, 3, 4, 5, 6, 7, 8, 9);
    long sum = a.a + a.b + a.c + b.a + c.a + c.b + d.a + d.b + d.c + e.a + e.b;
    sum = sum + f.a + f.e + g.a + g.b + g.c + h.a + h.b + i.a + i.b + i.c;
    struct s12 j = ret12(3);
    struct s24 k = ret24(2);
    sum = sum + j.c + k.b;
    return (int)(sum % 127);
}
