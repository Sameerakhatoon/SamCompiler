/* gcc side of the float struct ABI interop test (tests/390-float-struct-interop.sh);
   sam.c mirrors it. Structs with float and double members passed by value and
   returned both ways, after the floating-point or integer argument registers
   are used up, and through `...`. */
#include <stdarg.h>
#include <stdio.h>
struct pt { float x; float y; };
union dl_u { double d; long l; };
struct s_d1 { double a; };
struct s_f1 { float a; };
struct s_f2 { float a; float b; };
struct s_d2 { double a; double b; };
struct s_f3 { float a; float b; float c; };
struct s_f4 { float a; float b; float c; float d; };
struct s_d3 { double a[3]; };
struct s_d4 { double a; double b; double c; double d; };
struct s_dl { double d; long l; };
struct s_ld { long l; double d; };
struct s_fi { float f; int i; };
struct s_if { int i; float f; };
struct s_cd { char c; double d; };
struct s_fd { float f; double d; };
struct s_ffi { float a; float b; int c; };
struct s_di { double d; int i; };
struct s_nest { struct pt p; float z; };
struct s_ud { union dl_u u; double e; };

struct s_d1 sam_make_d1(int k);
double sam_sum_d1(struct s_d1 x);
struct s_d1 sam_twice_d1(struct s_d1 x);
double sam_fsum_d1(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d1 x, double z);
double sam_isum_d1(long a, long b, long c, long d, long e, long f, struct s_d1 x, long g);
double sam_vsum_d1(int n, ...);
struct s_f1 sam_make_f1(int k);
double sam_sum_f1(struct s_f1 x);
struct s_f1 sam_twice_f1(struct s_f1 x);
double sam_fsum_f1(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f1 x, double z);
double sam_isum_f1(long a, long b, long c, long d, long e, long f, struct s_f1 x, long g);
double sam_vsum_f1(int n, ...);
struct s_f2 sam_make_f2(int k);
double sam_sum_f2(struct s_f2 x);
struct s_f2 sam_twice_f2(struct s_f2 x);
double sam_fsum_f2(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f2 x, double z);
double sam_isum_f2(long a, long b, long c, long d, long e, long f, struct s_f2 x, long g);
double sam_vsum_f2(int n, ...);
struct s_d2 sam_make_d2(int k);
double sam_sum_d2(struct s_d2 x);
struct s_d2 sam_twice_d2(struct s_d2 x);
double sam_fsum_d2(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d2 x, double z);
double sam_isum_d2(long a, long b, long c, long d, long e, long f, struct s_d2 x, long g);
double sam_vsum_d2(int n, ...);
struct s_f3 sam_make_f3(int k);
double sam_sum_f3(struct s_f3 x);
struct s_f3 sam_twice_f3(struct s_f3 x);
double sam_fsum_f3(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f3 x, double z);
double sam_isum_f3(long a, long b, long c, long d, long e, long f, struct s_f3 x, long g);
double sam_vsum_f3(int n, ...);
struct s_f4 sam_make_f4(int k);
double sam_sum_f4(struct s_f4 x);
struct s_f4 sam_twice_f4(struct s_f4 x);
double sam_fsum_f4(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f4 x, double z);
double sam_isum_f4(long a, long b, long c, long d, long e, long f, struct s_f4 x, long g);
double sam_vsum_f4(int n, ...);
struct s_d3 sam_make_d3(int k);
double sam_sum_d3(struct s_d3 x);
struct s_d3 sam_twice_d3(struct s_d3 x);
double sam_fsum_d3(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d3 x, double z);
double sam_isum_d3(long a, long b, long c, long d, long e, long f, struct s_d3 x, long g);
double sam_vsum_d3(int n, ...);
struct s_d4 sam_make_d4(int k);
double sam_sum_d4(struct s_d4 x);
struct s_d4 sam_twice_d4(struct s_d4 x);
double sam_fsum_d4(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d4 x, double z);
double sam_isum_d4(long a, long b, long c, long d, long e, long f, struct s_d4 x, long g);
double sam_vsum_d4(int n, ...);
struct s_dl sam_make_dl(int k);
double sam_sum_dl(struct s_dl x);
struct s_dl sam_twice_dl(struct s_dl x);
double sam_fsum_dl(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_dl x, double z);
double sam_isum_dl(long a, long b, long c, long d, long e, long f, struct s_dl x, long g);
double sam_vsum_dl(int n, ...);
struct s_ld sam_make_ld(int k);
double sam_sum_ld(struct s_ld x);
struct s_ld sam_twice_ld(struct s_ld x);
double sam_fsum_ld(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_ld x, double z);
double sam_isum_ld(long a, long b, long c, long d, long e, long f, struct s_ld x, long g);
double sam_vsum_ld(int n, ...);
struct s_fi sam_make_fi(int k);
double sam_sum_fi(struct s_fi x);
struct s_fi sam_twice_fi(struct s_fi x);
double sam_fsum_fi(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_fi x, double z);
double sam_isum_fi(long a, long b, long c, long d, long e, long f, struct s_fi x, long g);
double sam_vsum_fi(int n, ...);
struct s_if sam_make_if(int k);
double sam_sum_if(struct s_if x);
struct s_if sam_twice_if(struct s_if x);
double sam_fsum_if(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_if x, double z);
double sam_isum_if(long a, long b, long c, long d, long e, long f, struct s_if x, long g);
double sam_vsum_if(int n, ...);
struct s_cd sam_make_cd(int k);
double sam_sum_cd(struct s_cd x);
struct s_cd sam_twice_cd(struct s_cd x);
double sam_fsum_cd(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_cd x, double z);
double sam_isum_cd(long a, long b, long c, long d, long e, long f, struct s_cd x, long g);
double sam_vsum_cd(int n, ...);
struct s_fd sam_make_fd(int k);
double sam_sum_fd(struct s_fd x);
struct s_fd sam_twice_fd(struct s_fd x);
double sam_fsum_fd(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_fd x, double z);
double sam_isum_fd(long a, long b, long c, long d, long e, long f, struct s_fd x, long g);
double sam_vsum_fd(int n, ...);
struct s_ffi sam_make_ffi(int k);
double sam_sum_ffi(struct s_ffi x);
struct s_ffi sam_twice_ffi(struct s_ffi x);
double sam_fsum_ffi(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_ffi x, double z);
double sam_isum_ffi(long a, long b, long c, long d, long e, long f, struct s_ffi x, long g);
double sam_vsum_ffi(int n, ...);
struct s_di sam_make_di(int k);
double sam_sum_di(struct s_di x);
struct s_di sam_twice_di(struct s_di x);
double sam_fsum_di(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_di x, double z);
double sam_isum_di(long a, long b, long c, long d, long e, long f, struct s_di x, long g);
double sam_vsum_di(int n, ...);
struct s_nest sam_make_nest(int k);
double sam_sum_nest(struct s_nest x);
struct s_nest sam_twice_nest(struct s_nest x);
double sam_fsum_nest(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_nest x, double z);
double sam_isum_nest(long a, long b, long c, long d, long e, long f, struct s_nest x, long g);
double sam_vsum_nest(int n, ...);
struct s_ud sam_make_ud(int k);
double sam_sum_ud(struct s_ud x);
struct s_ud sam_twice_ud(struct s_ud x);
double sam_fsum_ud(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_ud x, double z);
double sam_isum_ud(long a, long b, long c, long d, long e, long f, struct s_ud x, long g);
double sam_vsum_ud(int n, ...);

static void fill_d1(struct s_d1* p, int k){
    p->a = (k + 1 * 0.5);
}
static double weigh_d1(struct s_d1* p){
    double r = 0;
    r = r + 1 * p->a;
    return r;
}
static int same_d1(struct s_d1* p, struct s_d1* q, int times){
    return p->a == q->a * times;
}
static void fill_f1(struct s_f1* p, int k){
    p->a = (k + 1 * 0.5);
}
static double weigh_f1(struct s_f1* p){
    double r = 0;
    r = r + 1 * p->a;
    return r;
}
static int same_f1(struct s_f1* p, struct s_f1* q, int times){
    return p->a == q->a * times;
}
static void fill_f2(struct s_f2* p, int k){
    p->a = (k + 1 * 0.5);
    p->b = (k + 2 * 0.5);
}
static double weigh_f2(struct s_f2* p){
    double r = 0;
    r = r + 1 * p->a;
    r = r + 2 * p->b;
    return r;
}
static int same_f2(struct s_f2* p, struct s_f2* q, int times){
    return p->a == q->a * times && p->b == q->b * times;
}
static void fill_d2(struct s_d2* p, int k){
    p->a = (k + 1 * 0.5);
    p->b = (k + 2 * 0.5);
}
static double weigh_d2(struct s_d2* p){
    double r = 0;
    r = r + 1 * p->a;
    r = r + 2 * p->b;
    return r;
}
static int same_d2(struct s_d2* p, struct s_d2* q, int times){
    return p->a == q->a * times && p->b == q->b * times;
}
static void fill_f3(struct s_f3* p, int k){
    p->a = (k + 1 * 0.5);
    p->b = (k + 2 * 0.5);
    p->c = (k + 3 * 0.5);
}
static double weigh_f3(struct s_f3* p){
    double r = 0;
    r = r + 1 * p->a;
    r = r + 2 * p->b;
    r = r + 3 * p->c;
    return r;
}
static int same_f3(struct s_f3* p, struct s_f3* q, int times){
    return p->a == q->a * times && p->b == q->b * times && p->c == q->c * times;
}
static void fill_f4(struct s_f4* p, int k){
    p->a = (k + 1 * 0.5);
    p->b = (k + 2 * 0.5);
    p->c = (k + 3 * 0.5);
    p->d = (k + 4 * 0.5);
}
static double weigh_f4(struct s_f4* p){
    double r = 0;
    r = r + 1 * p->a;
    r = r + 2 * p->b;
    r = r + 3 * p->c;
    r = r + 4 * p->d;
    return r;
}
static int same_f4(struct s_f4* p, struct s_f4* q, int times){
    return p->a == q->a * times && p->b == q->b * times && p->c == q->c * times && p->d == q->d * times;
}
static void fill_d3(struct s_d3* p, int k){
    p->a[0] = (k + 1 * 0.5);
    p->a[1] = (k + 2 * 0.5);
    p->a[2] = (k + 3 * 0.5);
}
static double weigh_d3(struct s_d3* p){
    double r = 0;
    r = r + 1 * p->a[0];
    r = r + 2 * p->a[1];
    r = r + 3 * p->a[2];
    return r;
}
static int same_d3(struct s_d3* p, struct s_d3* q, int times){
    return p->a[0] == q->a[0] * times && p->a[1] == q->a[1] * times && p->a[2] == q->a[2] * times;
}
static void fill_d4(struct s_d4* p, int k){
    p->a = (k + 1 * 0.5);
    p->b = (k + 2 * 0.5);
    p->c = (k + 3 * 0.5);
    p->d = (k + 4 * 0.5);
}
static double weigh_d4(struct s_d4* p){
    double r = 0;
    r = r + 1 * p->a;
    r = r + 2 * p->b;
    r = r + 3 * p->c;
    r = r + 4 * p->d;
    return r;
}
static int same_d4(struct s_d4* p, struct s_d4* q, int times){
    return p->a == q->a * times && p->b == q->b * times && p->c == q->c * times && p->d == q->d * times;
}
static void fill_dl(struct s_dl* p, int k){
    p->d = (k + 1 * 0.5);
    p->l = (k * 3 + 2);
}
static double weigh_dl(struct s_dl* p){
    double r = 0;
    r = r + 1 * p->d;
    r = r + 2 * p->l;
    return r;
}
static int same_dl(struct s_dl* p, struct s_dl* q, int times){
    return p->d == q->d * times && p->l == q->l * times;
}
static void fill_ld(struct s_ld* p, int k){
    p->l = (k * 3 + 1);
    p->d = (k + 2 * 0.5);
}
static double weigh_ld(struct s_ld* p){
    double r = 0;
    r = r + 1 * p->l;
    r = r + 2 * p->d;
    return r;
}
static int same_ld(struct s_ld* p, struct s_ld* q, int times){
    return p->l == q->l * times && p->d == q->d * times;
}
static void fill_fi(struct s_fi* p, int k){
    p->f = (k + 1 * 0.5);
    p->i = (k * 3 + 2);
}
static double weigh_fi(struct s_fi* p){
    double r = 0;
    r = r + 1 * p->f;
    r = r + 2 * p->i;
    return r;
}
static int same_fi(struct s_fi* p, struct s_fi* q, int times){
    return p->f == q->f * times && p->i == q->i * times;
}
static void fill_if(struct s_if* p, int k){
    p->i = (k * 3 + 1);
    p->f = (k + 2 * 0.5);
}
static double weigh_if(struct s_if* p){
    double r = 0;
    r = r + 1 * p->i;
    r = r + 2 * p->f;
    return r;
}
static int same_if(struct s_if* p, struct s_if* q, int times){
    return p->i == q->i * times && p->f == q->f * times;
}
static void fill_cd(struct s_cd* p, int k){
    p->c = (k * 3 + 1);
    p->d = (k + 2 * 0.5);
}
static double weigh_cd(struct s_cd* p){
    double r = 0;
    r = r + 1 * p->c;
    r = r + 2 * p->d;
    return r;
}
static int same_cd(struct s_cd* p, struct s_cd* q, int times){
    return p->c == q->c * times && p->d == q->d * times;
}
static void fill_fd(struct s_fd* p, int k){
    p->f = (k + 1 * 0.5);
    p->d = (k + 2 * 0.5);
}
static double weigh_fd(struct s_fd* p){
    double r = 0;
    r = r + 1 * p->f;
    r = r + 2 * p->d;
    return r;
}
static int same_fd(struct s_fd* p, struct s_fd* q, int times){
    return p->f == q->f * times && p->d == q->d * times;
}
static void fill_ffi(struct s_ffi* p, int k){
    p->a = (k + 1 * 0.5);
    p->b = (k + 2 * 0.5);
    p->c = (k * 3 + 3);
}
static double weigh_ffi(struct s_ffi* p){
    double r = 0;
    r = r + 1 * p->a;
    r = r + 2 * p->b;
    r = r + 3 * p->c;
    return r;
}
static int same_ffi(struct s_ffi* p, struct s_ffi* q, int times){
    return p->a == q->a * times && p->b == q->b * times && p->c == q->c * times;
}
static void fill_di(struct s_di* p, int k){
    p->d = (k + 1 * 0.5);
    p->i = (k * 3 + 2);
}
static double weigh_di(struct s_di* p){
    double r = 0;
    r = r + 1 * p->d;
    r = r + 2 * p->i;
    return r;
}
static int same_di(struct s_di* p, struct s_di* q, int times){
    return p->d == q->d * times && p->i == q->i * times;
}
static void fill_nest(struct s_nest* p, int k){
    p->p.x = (k + 1 * 0.5);
    p->p.y = (k + 2 * 0.5);
    p->z = (k + 3 * 0.5);
}
static double weigh_nest(struct s_nest* p){
    double r = 0;
    r = r + 1 * p->p.x;
    r = r + 2 * p->p.y;
    r = r + 3 * p->z;
    return r;
}
static int same_nest(struct s_nest* p, struct s_nest* q, int times){
    return p->p.x == q->p.x * times && p->p.y == q->p.y * times && p->z == q->z * times;
}
static void fill_ud(struct s_ud* p, int k){
    p->u.l = (k * 3 + 1);
    p->e = (k + 2 * 0.5);
}
static double weigh_ud(struct s_ud* p){
    double r = 0;
    r = r + 1 * p->u.l;
    r = r + 2 * p->e;
    return r;
}
static int same_ud(struct s_ud* p, struct s_ud* q, int times){
    return p->u.l == q->u.l * times && p->e == q->e * times;
}

struct s_d1 gcc_make_d1(int k){ struct s_d1 r; fill_d1(&r, k); return r; }
double gcc_sum_d1(struct s_d1 x){ return weigh_d1(&x); }
struct s_d1 gcc_twice_d1(struct s_d1 x){ x.a = x.a * 2; return x; }
double gcc_fsum_d1(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d1 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_d1(&x) + z * 100000;
}
double gcc_isum_d1(long a, long b, long c, long d, long e, long f, struct s_d1 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_d1(&x) + g * 100000;
}
double gcc_vsum_d1(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_d1 x = va_arg(ap, struct s_d1);
        r = r * 3 + weigh_d1(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_f1 gcc_make_f1(int k){ struct s_f1 r; fill_f1(&r, k); return r; }
double gcc_sum_f1(struct s_f1 x){ return weigh_f1(&x); }
struct s_f1 gcc_twice_f1(struct s_f1 x){ x.a = x.a * 2; return x; }
double gcc_fsum_f1(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f1 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_f1(&x) + z * 100000;
}
double gcc_isum_f1(long a, long b, long c, long d, long e, long f, struct s_f1 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_f1(&x) + g * 100000;
}
double gcc_vsum_f1(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_f1 x = va_arg(ap, struct s_f1);
        r = r * 3 + weigh_f1(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_f2 gcc_make_f2(int k){ struct s_f2 r; fill_f2(&r, k); return r; }
double gcc_sum_f2(struct s_f2 x){ return weigh_f2(&x); }
struct s_f2 gcc_twice_f2(struct s_f2 x){ x.a = x.a * 2; x.b = x.b * 2; return x; }
double gcc_fsum_f2(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f2 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_f2(&x) + z * 100000;
}
double gcc_isum_f2(long a, long b, long c, long d, long e, long f, struct s_f2 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_f2(&x) + g * 100000;
}
double gcc_vsum_f2(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_f2 x = va_arg(ap, struct s_f2);
        r = r * 3 + weigh_f2(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_d2 gcc_make_d2(int k){ struct s_d2 r; fill_d2(&r, k); return r; }
double gcc_sum_d2(struct s_d2 x){ return weigh_d2(&x); }
struct s_d2 gcc_twice_d2(struct s_d2 x){ x.a = x.a * 2; x.b = x.b * 2; return x; }
double gcc_fsum_d2(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d2 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_d2(&x) + z * 100000;
}
double gcc_isum_d2(long a, long b, long c, long d, long e, long f, struct s_d2 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_d2(&x) + g * 100000;
}
double gcc_vsum_d2(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_d2 x = va_arg(ap, struct s_d2);
        r = r * 3 + weigh_d2(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_f3 gcc_make_f3(int k){ struct s_f3 r; fill_f3(&r, k); return r; }
double gcc_sum_f3(struct s_f3 x){ return weigh_f3(&x); }
struct s_f3 gcc_twice_f3(struct s_f3 x){ x.a = x.a * 2; x.b = x.b * 2; x.c = x.c * 2; return x; }
double gcc_fsum_f3(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f3 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_f3(&x) + z * 100000;
}
double gcc_isum_f3(long a, long b, long c, long d, long e, long f, struct s_f3 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_f3(&x) + g * 100000;
}
double gcc_vsum_f3(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_f3 x = va_arg(ap, struct s_f3);
        r = r * 3 + weigh_f3(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_f4 gcc_make_f4(int k){ struct s_f4 r; fill_f4(&r, k); return r; }
double gcc_sum_f4(struct s_f4 x){ return weigh_f4(&x); }
struct s_f4 gcc_twice_f4(struct s_f4 x){ x.a = x.a * 2; x.b = x.b * 2; x.c = x.c * 2; x.d = x.d * 2; return x; }
double gcc_fsum_f4(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_f4 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_f4(&x) + z * 100000;
}
double gcc_isum_f4(long a, long b, long c, long d, long e, long f, struct s_f4 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_f4(&x) + g * 100000;
}
double gcc_vsum_f4(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_f4 x = va_arg(ap, struct s_f4);
        r = r * 3 + weigh_f4(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_d3 gcc_make_d3(int k){ struct s_d3 r; fill_d3(&r, k); return r; }
double gcc_sum_d3(struct s_d3 x){ return weigh_d3(&x); }
struct s_d3 gcc_twice_d3(struct s_d3 x){ x.a[0] = x.a[0] * 2; x.a[1] = x.a[1] * 2; x.a[2] = x.a[2] * 2; return x; }
double gcc_fsum_d3(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d3 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_d3(&x) + z * 100000;
}
double gcc_isum_d3(long a, long b, long c, long d, long e, long f, struct s_d3 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_d3(&x) + g * 100000;
}
double gcc_vsum_d3(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_d3 x = va_arg(ap, struct s_d3);
        r = r * 3 + weigh_d3(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_d4 gcc_make_d4(int k){ struct s_d4 r; fill_d4(&r, k); return r; }
double gcc_sum_d4(struct s_d4 x){ return weigh_d4(&x); }
struct s_d4 gcc_twice_d4(struct s_d4 x){ x.a = x.a * 2; x.b = x.b * 2; x.c = x.c * 2; x.d = x.d * 2; return x; }
double gcc_fsum_d4(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_d4 x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_d4(&x) + z * 100000;
}
double gcc_isum_d4(long a, long b, long c, long d, long e, long f, struct s_d4 x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_d4(&x) + g * 100000;
}
double gcc_vsum_d4(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_d4 x = va_arg(ap, struct s_d4);
        r = r * 3 + weigh_d4(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_dl gcc_make_dl(int k){ struct s_dl r; fill_dl(&r, k); return r; }
double gcc_sum_dl(struct s_dl x){ return weigh_dl(&x); }
struct s_dl gcc_twice_dl(struct s_dl x){ x.d = x.d * 2; x.l = x.l * 2; return x; }
double gcc_fsum_dl(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_dl x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_dl(&x) + z * 100000;
}
double gcc_isum_dl(long a, long b, long c, long d, long e, long f, struct s_dl x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_dl(&x) + g * 100000;
}
double gcc_vsum_dl(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_dl x = va_arg(ap, struct s_dl);
        r = r * 3 + weigh_dl(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_ld gcc_make_ld(int k){ struct s_ld r; fill_ld(&r, k); return r; }
double gcc_sum_ld(struct s_ld x){ return weigh_ld(&x); }
struct s_ld gcc_twice_ld(struct s_ld x){ x.l = x.l * 2; x.d = x.d * 2; return x; }
double gcc_fsum_ld(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_ld x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_ld(&x) + z * 100000;
}
double gcc_isum_ld(long a, long b, long c, long d, long e, long f, struct s_ld x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_ld(&x) + g * 100000;
}
double gcc_vsum_ld(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_ld x = va_arg(ap, struct s_ld);
        r = r * 3 + weigh_ld(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_fi gcc_make_fi(int k){ struct s_fi r; fill_fi(&r, k); return r; }
double gcc_sum_fi(struct s_fi x){ return weigh_fi(&x); }
struct s_fi gcc_twice_fi(struct s_fi x){ x.f = x.f * 2; x.i = x.i * 2; return x; }
double gcc_fsum_fi(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_fi x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_fi(&x) + z * 100000;
}
double gcc_isum_fi(long a, long b, long c, long d, long e, long f, struct s_fi x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_fi(&x) + g * 100000;
}
double gcc_vsum_fi(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_fi x = va_arg(ap, struct s_fi);
        r = r * 3 + weigh_fi(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_if gcc_make_if(int k){ struct s_if r; fill_if(&r, k); return r; }
double gcc_sum_if(struct s_if x){ return weigh_if(&x); }
struct s_if gcc_twice_if(struct s_if x){ x.i = x.i * 2; x.f = x.f * 2; return x; }
double gcc_fsum_if(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_if x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_if(&x) + z * 100000;
}
double gcc_isum_if(long a, long b, long c, long d, long e, long f, struct s_if x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_if(&x) + g * 100000;
}
double gcc_vsum_if(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_if x = va_arg(ap, struct s_if);
        r = r * 3 + weigh_if(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_cd gcc_make_cd(int k){ struct s_cd r; fill_cd(&r, k); return r; }
double gcc_sum_cd(struct s_cd x){ return weigh_cd(&x); }
struct s_cd gcc_twice_cd(struct s_cd x){ x.c = x.c * 2; x.d = x.d * 2; return x; }
double gcc_fsum_cd(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_cd x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_cd(&x) + z * 100000;
}
double gcc_isum_cd(long a, long b, long c, long d, long e, long f, struct s_cd x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_cd(&x) + g * 100000;
}
double gcc_vsum_cd(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_cd x = va_arg(ap, struct s_cd);
        r = r * 3 + weigh_cd(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_fd gcc_make_fd(int k){ struct s_fd r; fill_fd(&r, k); return r; }
double gcc_sum_fd(struct s_fd x){ return weigh_fd(&x); }
struct s_fd gcc_twice_fd(struct s_fd x){ x.f = x.f * 2; x.d = x.d * 2; return x; }
double gcc_fsum_fd(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_fd x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_fd(&x) + z * 100000;
}
double gcc_isum_fd(long a, long b, long c, long d, long e, long f, struct s_fd x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_fd(&x) + g * 100000;
}
double gcc_vsum_fd(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_fd x = va_arg(ap, struct s_fd);
        r = r * 3 + weigh_fd(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_ffi gcc_make_ffi(int k){ struct s_ffi r; fill_ffi(&r, k); return r; }
double gcc_sum_ffi(struct s_ffi x){ return weigh_ffi(&x); }
struct s_ffi gcc_twice_ffi(struct s_ffi x){ x.a = x.a * 2; x.b = x.b * 2; x.c = x.c * 2; return x; }
double gcc_fsum_ffi(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_ffi x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_ffi(&x) + z * 100000;
}
double gcc_isum_ffi(long a, long b, long c, long d, long e, long f, struct s_ffi x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_ffi(&x) + g * 100000;
}
double gcc_vsum_ffi(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_ffi x = va_arg(ap, struct s_ffi);
        r = r * 3 + weigh_ffi(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_di gcc_make_di(int k){ struct s_di r; fill_di(&r, k); return r; }
double gcc_sum_di(struct s_di x){ return weigh_di(&x); }
struct s_di gcc_twice_di(struct s_di x){ x.d = x.d * 2; x.i = x.i * 2; return x; }
double gcc_fsum_di(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_di x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_di(&x) + z * 100000;
}
double gcc_isum_di(long a, long b, long c, long d, long e, long f, struct s_di x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_di(&x) + g * 100000;
}
double gcc_vsum_di(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_di x = va_arg(ap, struct s_di);
        r = r * 3 + weigh_di(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_nest gcc_make_nest(int k){ struct s_nest r; fill_nest(&r, k); return r; }
double gcc_sum_nest(struct s_nest x){ return weigh_nest(&x); }
struct s_nest gcc_twice_nest(struct s_nest x){ x.p.x = x.p.x * 2; x.p.y = x.p.y * 2; x.z = x.z * 2; return x; }
double gcc_fsum_nest(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_nest x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_nest(&x) + z * 100000;
}
double gcc_isum_nest(long a, long b, long c, long d, long e, long f, struct s_nest x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_nest(&x) + g * 100000;
}
double gcc_vsum_nest(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_nest x = va_arg(ap, struct s_nest);
        r = r * 3 + weigh_nest(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}
struct s_ud gcc_make_ud(int k){ struct s_ud r; fill_ud(&r, k); return r; }
double gcc_sum_ud(struct s_ud x){ return weigh_ud(&x); }
struct s_ud gcc_twice_ud(struct s_ud x){ x.u.l = x.u.l * 2; x.e = x.e * 2; return x; }
double gcc_fsum_ud(double d0, double d1, double d2, double d3, double d4, double d5, double d6, struct s_ud x, double z){
    return (d0 + d1 * 2 + d2 * 3 + d3 * 4 + d4 * 5 + d5 * 6 + d6 * 7) * 1000 + weigh_ud(&x) + z * 100000;
}
double gcc_isum_ud(long a, long b, long c, long d, long e, long f, struct s_ud x, long g){
    return (a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6) * 1000 + weigh_ud(&x) + g * 100000;
}
double gcc_vsum_ud(int n, ...){
    va_list ap;
    double r = 0;
    va_start(ap, n);
    for(int i = 0; i < n; i++){
        struct s_ud x = va_arg(ap, struct s_ud);
        r = r * 3 + weigh_ud(&x);
        r = r + va_arg(ap, double);
    }
    va_end(ap);
    return r;
}

int gcc_check(void){
    int fail = 0;
    {
        struct s_d1 want; struct s_d1 got; struct s_d1 a; struct s_d1 b;
        fill_d1(&want, 3);
        got = sam_make_d1(3);
        if(!same_d1(&got, &want, 1)){ printf("FAIL sam make_d1\n"); fail++; }
        fill_d1(&a, 5);
        if(sam_sum_d1(a) != weigh_d1(&a)){ printf("FAIL sam sum_d1\n"); fail++; }
        b = sam_twice_d1(a);
        if(!same_d1(&b, &a, 2)){ printf("FAIL sam twice_d1\n"); fail++; }
        if(sam_fsum_d1(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_d1(&a) + 50000){ printf("FAIL sam fsum_d1\n"); fail++; }
        if(sam_isum_d1(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_d1(&a) + 700000){ printf("FAIL sam isum_d1\n"); fail++; }
        fill_d1(&b, 7);
        if(sam_vsum_d1(2, a, 0.25, b, 0.75) != (weigh_d1(&a) + 0.25) * 3 + weigh_d1(&b) + 0.75){ printf("FAIL sam vsum_d1\n"); fail++; }
    }
    {
        struct s_f1 want; struct s_f1 got; struct s_f1 a; struct s_f1 b;
        fill_f1(&want, 3);
        got = sam_make_f1(3);
        if(!same_f1(&got, &want, 1)){ printf("FAIL sam make_f1\n"); fail++; }
        fill_f1(&a, 5);
        if(sam_sum_f1(a) != weigh_f1(&a)){ printf("FAIL sam sum_f1\n"); fail++; }
        b = sam_twice_f1(a);
        if(!same_f1(&b, &a, 2)){ printf("FAIL sam twice_f1\n"); fail++; }
        if(sam_fsum_f1(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_f1(&a) + 50000){ printf("FAIL sam fsum_f1\n"); fail++; }
        if(sam_isum_f1(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_f1(&a) + 700000){ printf("FAIL sam isum_f1\n"); fail++; }
        fill_f1(&b, 7);
        if(sam_vsum_f1(2, a, 0.25, b, 0.75) != (weigh_f1(&a) + 0.25) * 3 + weigh_f1(&b) + 0.75){ printf("FAIL sam vsum_f1\n"); fail++; }
    }
    {
        struct s_f2 want; struct s_f2 got; struct s_f2 a; struct s_f2 b;
        fill_f2(&want, 3);
        got = sam_make_f2(3);
        if(!same_f2(&got, &want, 1)){ printf("FAIL sam make_f2\n"); fail++; }
        fill_f2(&a, 5);
        if(sam_sum_f2(a) != weigh_f2(&a)){ printf("FAIL sam sum_f2\n"); fail++; }
        b = sam_twice_f2(a);
        if(!same_f2(&b, &a, 2)){ printf("FAIL sam twice_f2\n"); fail++; }
        if(sam_fsum_f2(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_f2(&a) + 50000){ printf("FAIL sam fsum_f2\n"); fail++; }
        if(sam_isum_f2(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_f2(&a) + 700000){ printf("FAIL sam isum_f2\n"); fail++; }
        fill_f2(&b, 7);
        if(sam_vsum_f2(2, a, 0.25, b, 0.75) != (weigh_f2(&a) + 0.25) * 3 + weigh_f2(&b) + 0.75){ printf("FAIL sam vsum_f2\n"); fail++; }
    }
    {
        struct s_d2 want; struct s_d2 got; struct s_d2 a; struct s_d2 b;
        fill_d2(&want, 3);
        got = sam_make_d2(3);
        if(!same_d2(&got, &want, 1)){ printf("FAIL sam make_d2\n"); fail++; }
        fill_d2(&a, 5);
        if(sam_sum_d2(a) != weigh_d2(&a)){ printf("FAIL sam sum_d2\n"); fail++; }
        b = sam_twice_d2(a);
        if(!same_d2(&b, &a, 2)){ printf("FAIL sam twice_d2\n"); fail++; }
        if(sam_fsum_d2(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_d2(&a) + 50000){ printf("FAIL sam fsum_d2\n"); fail++; }
        if(sam_isum_d2(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_d2(&a) + 700000){ printf("FAIL sam isum_d2\n"); fail++; }
        fill_d2(&b, 7);
        if(sam_vsum_d2(2, a, 0.25, b, 0.75) != (weigh_d2(&a) + 0.25) * 3 + weigh_d2(&b) + 0.75){ printf("FAIL sam vsum_d2\n"); fail++; }
    }
    {
        struct s_f3 want; struct s_f3 got; struct s_f3 a; struct s_f3 b;
        fill_f3(&want, 3);
        got = sam_make_f3(3);
        if(!same_f3(&got, &want, 1)){ printf("FAIL sam make_f3\n"); fail++; }
        fill_f3(&a, 5);
        if(sam_sum_f3(a) != weigh_f3(&a)){ printf("FAIL sam sum_f3\n"); fail++; }
        b = sam_twice_f3(a);
        if(!same_f3(&b, &a, 2)){ printf("FAIL sam twice_f3\n"); fail++; }
        if(sam_fsum_f3(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_f3(&a) + 50000){ printf("FAIL sam fsum_f3\n"); fail++; }
        if(sam_isum_f3(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_f3(&a) + 700000){ printf("FAIL sam isum_f3\n"); fail++; }
        fill_f3(&b, 7);
        if(sam_vsum_f3(2, a, 0.25, b, 0.75) != (weigh_f3(&a) + 0.25) * 3 + weigh_f3(&b) + 0.75){ printf("FAIL sam vsum_f3\n"); fail++; }
    }
    {
        struct s_f4 want; struct s_f4 got; struct s_f4 a; struct s_f4 b;
        fill_f4(&want, 3);
        got = sam_make_f4(3);
        if(!same_f4(&got, &want, 1)){ printf("FAIL sam make_f4\n"); fail++; }
        fill_f4(&a, 5);
        if(sam_sum_f4(a) != weigh_f4(&a)){ printf("FAIL sam sum_f4\n"); fail++; }
        b = sam_twice_f4(a);
        if(!same_f4(&b, &a, 2)){ printf("FAIL sam twice_f4\n"); fail++; }
        if(sam_fsum_f4(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_f4(&a) + 50000){ printf("FAIL sam fsum_f4\n"); fail++; }
        if(sam_isum_f4(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_f4(&a) + 700000){ printf("FAIL sam isum_f4\n"); fail++; }
        fill_f4(&b, 7);
        if(sam_vsum_f4(2, a, 0.25, b, 0.75) != (weigh_f4(&a) + 0.25) * 3 + weigh_f4(&b) + 0.75){ printf("FAIL sam vsum_f4\n"); fail++; }
    }
    {
        struct s_d3 want; struct s_d3 got; struct s_d3 a; struct s_d3 b;
        fill_d3(&want, 3);
        got = sam_make_d3(3);
        if(!same_d3(&got, &want, 1)){ printf("FAIL sam make_d3\n"); fail++; }
        fill_d3(&a, 5);
        if(sam_sum_d3(a) != weigh_d3(&a)){ printf("FAIL sam sum_d3\n"); fail++; }
        b = sam_twice_d3(a);
        if(!same_d3(&b, &a, 2)){ printf("FAIL sam twice_d3\n"); fail++; }
        if(sam_fsum_d3(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_d3(&a) + 50000){ printf("FAIL sam fsum_d3\n"); fail++; }
        if(sam_isum_d3(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_d3(&a) + 700000){ printf("FAIL sam isum_d3\n"); fail++; }
        fill_d3(&b, 7);
        if(sam_vsum_d3(2, a, 0.25, b, 0.75) != (weigh_d3(&a) + 0.25) * 3 + weigh_d3(&b) + 0.75){ printf("FAIL sam vsum_d3\n"); fail++; }
    }
    {
        struct s_d4 want; struct s_d4 got; struct s_d4 a; struct s_d4 b;
        fill_d4(&want, 3);
        got = sam_make_d4(3);
        if(!same_d4(&got, &want, 1)){ printf("FAIL sam make_d4\n"); fail++; }
        fill_d4(&a, 5);
        if(sam_sum_d4(a) != weigh_d4(&a)){ printf("FAIL sam sum_d4\n"); fail++; }
        b = sam_twice_d4(a);
        if(!same_d4(&b, &a, 2)){ printf("FAIL sam twice_d4\n"); fail++; }
        if(sam_fsum_d4(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_d4(&a) + 50000){ printf("FAIL sam fsum_d4\n"); fail++; }
        if(sam_isum_d4(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_d4(&a) + 700000){ printf("FAIL sam isum_d4\n"); fail++; }
        fill_d4(&b, 7);
        if(sam_vsum_d4(2, a, 0.25, b, 0.75) != (weigh_d4(&a) + 0.25) * 3 + weigh_d4(&b) + 0.75){ printf("FAIL sam vsum_d4\n"); fail++; }
    }
    {
        struct s_dl want; struct s_dl got; struct s_dl a; struct s_dl b;
        fill_dl(&want, 3);
        got = sam_make_dl(3);
        if(!same_dl(&got, &want, 1)){ printf("FAIL sam make_dl\n"); fail++; }
        fill_dl(&a, 5);
        if(sam_sum_dl(a) != weigh_dl(&a)){ printf("FAIL sam sum_dl\n"); fail++; }
        b = sam_twice_dl(a);
        if(!same_dl(&b, &a, 2)){ printf("FAIL sam twice_dl\n"); fail++; }
        if(sam_fsum_dl(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_dl(&a) + 50000){ printf("FAIL sam fsum_dl\n"); fail++; }
        if(sam_isum_dl(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_dl(&a) + 700000){ printf("FAIL sam isum_dl\n"); fail++; }
        fill_dl(&b, 7);
        if(sam_vsum_dl(2, a, 0.25, b, 0.75) != (weigh_dl(&a) + 0.25) * 3 + weigh_dl(&b) + 0.75){ printf("FAIL sam vsum_dl\n"); fail++; }
    }
    {
        struct s_ld want; struct s_ld got; struct s_ld a; struct s_ld b;
        fill_ld(&want, 3);
        got = sam_make_ld(3);
        if(!same_ld(&got, &want, 1)){ printf("FAIL sam make_ld\n"); fail++; }
        fill_ld(&a, 5);
        if(sam_sum_ld(a) != weigh_ld(&a)){ printf("FAIL sam sum_ld\n"); fail++; }
        b = sam_twice_ld(a);
        if(!same_ld(&b, &a, 2)){ printf("FAIL sam twice_ld\n"); fail++; }
        if(sam_fsum_ld(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_ld(&a) + 50000){ printf("FAIL sam fsum_ld\n"); fail++; }
        if(sam_isum_ld(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_ld(&a) + 700000){ printf("FAIL sam isum_ld\n"); fail++; }
        fill_ld(&b, 7);
        if(sam_vsum_ld(2, a, 0.25, b, 0.75) != (weigh_ld(&a) + 0.25) * 3 + weigh_ld(&b) + 0.75){ printf("FAIL sam vsum_ld\n"); fail++; }
    }
    {
        struct s_fi want; struct s_fi got; struct s_fi a; struct s_fi b;
        fill_fi(&want, 3);
        got = sam_make_fi(3);
        if(!same_fi(&got, &want, 1)){ printf("FAIL sam make_fi\n"); fail++; }
        fill_fi(&a, 5);
        if(sam_sum_fi(a) != weigh_fi(&a)){ printf("FAIL sam sum_fi\n"); fail++; }
        b = sam_twice_fi(a);
        if(!same_fi(&b, &a, 2)){ printf("FAIL sam twice_fi\n"); fail++; }
        if(sam_fsum_fi(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_fi(&a) + 50000){ printf("FAIL sam fsum_fi\n"); fail++; }
        if(sam_isum_fi(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_fi(&a) + 700000){ printf("FAIL sam isum_fi\n"); fail++; }
        fill_fi(&b, 7);
        if(sam_vsum_fi(2, a, 0.25, b, 0.75) != (weigh_fi(&a) + 0.25) * 3 + weigh_fi(&b) + 0.75){ printf("FAIL sam vsum_fi\n"); fail++; }
    }
    {
        struct s_if want; struct s_if got; struct s_if a; struct s_if b;
        fill_if(&want, 3);
        got = sam_make_if(3);
        if(!same_if(&got, &want, 1)){ printf("FAIL sam make_if\n"); fail++; }
        fill_if(&a, 5);
        if(sam_sum_if(a) != weigh_if(&a)){ printf("FAIL sam sum_if\n"); fail++; }
        b = sam_twice_if(a);
        if(!same_if(&b, &a, 2)){ printf("FAIL sam twice_if\n"); fail++; }
        if(sam_fsum_if(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_if(&a) + 50000){ printf("FAIL sam fsum_if\n"); fail++; }
        if(sam_isum_if(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_if(&a) + 700000){ printf("FAIL sam isum_if\n"); fail++; }
        fill_if(&b, 7);
        if(sam_vsum_if(2, a, 0.25, b, 0.75) != (weigh_if(&a) + 0.25) * 3 + weigh_if(&b) + 0.75){ printf("FAIL sam vsum_if\n"); fail++; }
    }
    {
        struct s_cd want; struct s_cd got; struct s_cd a; struct s_cd b;
        fill_cd(&want, 3);
        got = sam_make_cd(3);
        if(!same_cd(&got, &want, 1)){ printf("FAIL sam make_cd\n"); fail++; }
        fill_cd(&a, 5);
        if(sam_sum_cd(a) != weigh_cd(&a)){ printf("FAIL sam sum_cd\n"); fail++; }
        b = sam_twice_cd(a);
        if(!same_cd(&b, &a, 2)){ printf("FAIL sam twice_cd\n"); fail++; }
        if(sam_fsum_cd(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_cd(&a) + 50000){ printf("FAIL sam fsum_cd\n"); fail++; }
        if(sam_isum_cd(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_cd(&a) + 700000){ printf("FAIL sam isum_cd\n"); fail++; }
        fill_cd(&b, 7);
        if(sam_vsum_cd(2, a, 0.25, b, 0.75) != (weigh_cd(&a) + 0.25) * 3 + weigh_cd(&b) + 0.75){ printf("FAIL sam vsum_cd\n"); fail++; }
    }
    {
        struct s_fd want; struct s_fd got; struct s_fd a; struct s_fd b;
        fill_fd(&want, 3);
        got = sam_make_fd(3);
        if(!same_fd(&got, &want, 1)){ printf("FAIL sam make_fd\n"); fail++; }
        fill_fd(&a, 5);
        if(sam_sum_fd(a) != weigh_fd(&a)){ printf("FAIL sam sum_fd\n"); fail++; }
        b = sam_twice_fd(a);
        if(!same_fd(&b, &a, 2)){ printf("FAIL sam twice_fd\n"); fail++; }
        if(sam_fsum_fd(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_fd(&a) + 50000){ printf("FAIL sam fsum_fd\n"); fail++; }
        if(sam_isum_fd(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_fd(&a) + 700000){ printf("FAIL sam isum_fd\n"); fail++; }
        fill_fd(&b, 7);
        if(sam_vsum_fd(2, a, 0.25, b, 0.75) != (weigh_fd(&a) + 0.25) * 3 + weigh_fd(&b) + 0.75){ printf("FAIL sam vsum_fd\n"); fail++; }
    }
    {
        struct s_ffi want; struct s_ffi got; struct s_ffi a; struct s_ffi b;
        fill_ffi(&want, 3);
        got = sam_make_ffi(3);
        if(!same_ffi(&got, &want, 1)){ printf("FAIL sam make_ffi\n"); fail++; }
        fill_ffi(&a, 5);
        if(sam_sum_ffi(a) != weigh_ffi(&a)){ printf("FAIL sam sum_ffi\n"); fail++; }
        b = sam_twice_ffi(a);
        if(!same_ffi(&b, &a, 2)){ printf("FAIL sam twice_ffi\n"); fail++; }
        if(sam_fsum_ffi(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_ffi(&a) + 50000){ printf("FAIL sam fsum_ffi\n"); fail++; }
        if(sam_isum_ffi(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_ffi(&a) + 700000){ printf("FAIL sam isum_ffi\n"); fail++; }
        fill_ffi(&b, 7);
        if(sam_vsum_ffi(2, a, 0.25, b, 0.75) != (weigh_ffi(&a) + 0.25) * 3 + weigh_ffi(&b) + 0.75){ printf("FAIL sam vsum_ffi\n"); fail++; }
    }
    {
        struct s_di want; struct s_di got; struct s_di a; struct s_di b;
        fill_di(&want, 3);
        got = sam_make_di(3);
        if(!same_di(&got, &want, 1)){ printf("FAIL sam make_di\n"); fail++; }
        fill_di(&a, 5);
        if(sam_sum_di(a) != weigh_di(&a)){ printf("FAIL sam sum_di\n"); fail++; }
        b = sam_twice_di(a);
        if(!same_di(&b, &a, 2)){ printf("FAIL sam twice_di\n"); fail++; }
        if(sam_fsum_di(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_di(&a) + 50000){ printf("FAIL sam fsum_di\n"); fail++; }
        if(sam_isum_di(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_di(&a) + 700000){ printf("FAIL sam isum_di\n"); fail++; }
        fill_di(&b, 7);
        if(sam_vsum_di(2, a, 0.25, b, 0.75) != (weigh_di(&a) + 0.25) * 3 + weigh_di(&b) + 0.75){ printf("FAIL sam vsum_di\n"); fail++; }
    }
    {
        struct s_nest want; struct s_nest got; struct s_nest a; struct s_nest b;
        fill_nest(&want, 3);
        got = sam_make_nest(3);
        if(!same_nest(&got, &want, 1)){ printf("FAIL sam make_nest\n"); fail++; }
        fill_nest(&a, 5);
        if(sam_sum_nest(a) != weigh_nest(&a)){ printf("FAIL sam sum_nest\n"); fail++; }
        b = sam_twice_nest(a);
        if(!same_nest(&b, &a, 2)){ printf("FAIL sam twice_nest\n"); fail++; }
        if(sam_fsum_nest(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_nest(&a) + 50000){ printf("FAIL sam fsum_nest\n"); fail++; }
        if(sam_isum_nest(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_nest(&a) + 700000){ printf("FAIL sam isum_nest\n"); fail++; }
        fill_nest(&b, 7);
        if(sam_vsum_nest(2, a, 0.25, b, 0.75) != (weigh_nest(&a) + 0.25) * 3 + weigh_nest(&b) + 0.75){ printf("FAIL sam vsum_nest\n"); fail++; }
    }
    {
        struct s_ud want; struct s_ud got; struct s_ud a; struct s_ud b;
        fill_ud(&want, 3);
        got = sam_make_ud(3);
        if(!same_ud(&got, &want, 1)){ printf("FAIL sam make_ud\n"); fail++; }
        fill_ud(&a, 5);
        if(sam_sum_ud(a) != weigh_ud(&a)){ printf("FAIL sam sum_ud\n"); fail++; }
        b = sam_twice_ud(a);
        if(!same_ud(&b, &a, 2)){ printf("FAIL sam twice_ud\n"); fail++; }
        if(sam_fsum_ud(1, 2, 3, 4, 5, 6, 7, a, 0.5) != 140000 + weigh_ud(&a) + 50000){ printf("FAIL sam fsum_ud\n"); fail++; }
        if(sam_isum_ud(1, 2, 3, 4, 5, 6, a, 7) != 91000 + weigh_ud(&a) + 700000){ printf("FAIL sam isum_ud\n"); fail++; }
        fill_ud(&b, 7);
        if(sam_vsum_ud(2, a, 0.25, b, 0.75) != (weigh_ud(&a) + 0.25) * 3 + weigh_ud(&b) + 0.75){ printf("FAIL sam vsum_ud\n"); fail++; }
    }
    return fail;
}

int sam_check(void);

int main(void){
    int fail = gcc_check() + sam_check();
    printf(fail ? "%d failed\n" : "ok\n", fail);
    return fail != 0;
}
