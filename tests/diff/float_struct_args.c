/* structs with float or double members by value between
   SamCompiler functions, now passed in floating-point registers by each
   psABI: results passed straight on, members of results inside
   expressions, calls through function pointers, registers running out,
   a parameter changed in the callee, and va_arg of such structs. */
#include <stdarg.h>

struct d2 { double a; double b; };
struct f3 { float x; float y; float z; };
struct dl { double d; long l; };
struct fi { float f; int i; };
struct d4 { double a[4]; };
struct pt { float x; float y; };
struct nest { struct pt p; float z; };

struct d2 d2_make(double a, double b){ struct d2 r; r.a = a; r.b = b; return r; }
struct d2 d2_swap(struct d2 v){ double t = v.a; v.a = v.b; v.b = t; return v; }
double d2_diff(struct d2 v){ return v.a - v.b; }
struct f3 f3_make(float x){ struct f3 r; r.x = x; r.y = x * 2; r.z = x * 3; return r; }
float f3_dot(struct f3 u, struct f3 v){ return u.x * v.x + u.y * v.y + u.z * v.z; }
struct dl dl_make(double d, long l){ struct dl r; r.d = d; r.l = l; return r; }
double dl_mix(struct dl v, int k){ return v.d * k + v.l; }
struct fi fi_make(float f, int i){ struct fi r; r.f = f; r.i = i; return r; }
double fi_mix(struct fi v){ v.i = v.i + 1; return v.f * 10 + v.i; }
struct d4 d4_scale(struct d4 v, double k){ for(int i = 0; i < 4; i++){ v.a[i] = v.a[i] * k; } return v; }
struct nest nest_make(float base){ struct nest n; n.p.x = base; n.p.y = base + 1; n.z = base + 2; return n; }
float nest_sum(struct nest n){ return n.p.x + n.p.y * 2 + n.z * 3; }

/* Seven doubles take xmm0-6 (v0-6, fa0-6): the struct does not fit in
   the one left, so it goes to the stack (x86_64, aarch64) or the
   integer registers (riscv64); z still takes the last register on
   x86_64 and riscv64, and the stack on aarch64. */
double crowded(double a, double b, double c, double d, double e, double f, double g, struct d2 s, double z){
    return a + b + c + d + e + f + g + s.a * 100 + s.b * 1000 + z * 10000;
}
double crowded_int(long a, long b, long c, long d, long e, long f, struct dl s, long g){
    return a + b + c + d + e + f + s.d * 100 + s.l * 1000 + g * 10000;
}

double vsum(int n, ...){
    va_list ap;
    va_start(ap, n);
    double r = 0;
    for(int i = 0; i < n; i++){
        struct d2 a = va_arg(ap, struct d2);
        struct dl b = va_arg(ap, struct dl);
        struct fi c = va_arg(ap, struct fi);
        r = r * 2 + a.a + a.b * 3 + b.d * 5 + b.l * 7 + c.f * 11 + c.i * 13;
    }
    va_end(ap);
    return r;
}

int main(void){
    int fail = 0;
    struct d2 v = d2_swap(d2_make(1.5, 4.25));
    if(v.a != 4.25 || v.b != 1.5) fail |= 1;
    if(d2_diff(d2_swap(d2_make(1, 3))) != 2) fail |= 2;
    if(2 + d2_diff(d2_make(0, 0.5)) != 1.5) fail |= 4;
    if(f3_dot(f3_make(1), f3_make(2)) != 28) fail |= 8;
    struct dl w = dl_make(0.5, -3);
    if(dl_mix(w, 4) != -1 || dl_mix(dl_make(1.25, 2), 2) != 4.5) fail |= 16;
    struct fi f = fi_make(0.5f, 6);
    if(fi_mix(f) != 12 || f.i != 6) fail |= 32;
    struct d4 q;
    for(int i = 0; i < 4; i++){ q.a[i] = i + 0.5; }
    q = d4_scale(q, 2);
    if(q.a[0] != 1 || q.a[3] != 7) fail |= 64;
    if(nest_sum(nest_make(1)) != 1 + 4 + 9) fail |= 128;
    double (*pd)(struct d2) = d2_diff;
    struct d2 (*pm)(double, double) = d2_make;
    if(pd(pm(5, 0.5)) != 4.5) fail |= 256;
    if(crowded(1, 2, 3, 4, 5, 6, 7, d2_make(0.5, 0.25), 2) != 28 + 50 + 250 + 20000) fail |= 512;
    if(crowded_int(1, 2, 3, 4, 5, 6, dl_make(0.5, 2), 3) != 21 + 50 + 2000 + 30000) fail |= 1024;
    if(vsum(2, d2_make(1, 2), dl_make(0.5, 1), fi_make(2, 1), d2_make(0, 1), dl_make(1, 0), fi_make(0, 2))
       != (1 + 6 + 2.5 + 7 + 22 + 13) * 2 + (3 + 5 + 26)) fail |= 2048;
    return fail % 255 + (fail > 254);
}
