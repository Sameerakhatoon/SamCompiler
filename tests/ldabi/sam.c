// SamCompiler side of the long double ABI interop test
// (tests/391-long-double-interop.sh); gcc.c is the gcc side.
#include <stdarg.h>
int printf(const char* fmt, ...);
int sprintf(char* out, const char* fmt, ...);
int strcmp(const char* a, const char* b);

struct ld1 { long double x; };
struct ld2 { long double a; long double b; };
struct ldk { long double x; int k; };

long double sam_global = 0.142857142857142857142857L;
extern long double gcc_global;

long double gcc_add(long double a, long double b);
long double gcc_many(long double a, long double b, long double c, long double d, long double e,
                     long double f, long double g, long double h, long double i, long double j);
long double gcc_mixed(int a, long double b, double c, float d, long double e, char f);
long double gcc_split(long a, long b, long c, long d, long e, long f, long g, long double x, int k);
long double gcc_vsum(int n, ...);
struct ld1 gcc_ld1(struct ld1 v);
struct ld2 gcc_ld2(struct ld2 v, long double k);
struct ldk gcc_ldk(struct ldk v);
long double gcc_global_value(void);

long double sam_add(long double a, long double b){ return a + b; }
long double sam_many(long double a, long double b, long double c, long double d, long double e,
                     long double f, long double g, long double h, long double i, long double j){
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}
long double sam_mixed(int a, long double b, double c, float d, long double e, char f){
    return a + b * 10 + c * 100 + d * 1000 + e * 10000 + f * 100000;
}
long double sam_split(long a, long b, long c, long d, long e, long f, long g, long double x, int k){
    return a + b + c + d + e + f + g + x * 100 + k * 1000;
}
long double sam_vsum(int n, ...){
    va_list ap;
    va_start(ap, n);
    long double s = 0;
    for(int i = 0; i < n; i++){
        s = s * 2 + va_arg(ap, long double);
        s = s + va_arg(ap, int);
        s = s + va_arg(ap, double);
    }
    va_end(ap);
    return s;
}
long double sam_from(double d, long l, unsigned long u){ return d + l + (long double)u; }
double sam_to_double(long double x){ return x; }
long sam_to_long(long double x){ return x; }
struct ld1 sam_ld1(struct ld1 v){ v.x = v.x * 2; return v; }
struct ld2 sam_ld2(struct ld2 v, long double k){ v.a = v.a * k; v.b = v.b + k; return v; }
struct ldk sam_ldk(struct ldk v){ v.x = -v.x; v.k = v.k + 1; return v; }
long double sam_global_value(void){ return sam_global; }

int sam_check(void){
    int fail = 0;
    if(gcc_add(1.5L, 0.1L) != 1.5L + 0.1L){ printf("FAIL gcc_add\n"); fail++; }
    if(gcc_many(1, 2, 3, 4, 5, 6, 7, 8, 9, 0.5L) != 290){ printf("FAIL gcc_many\n"); fail++; }
    if(gcc_mixed(1, 2, 3, 4, 5, 6) != 1 + 20 + 300 + 4000 + 50000 + 600000){ printf("FAIL gcc_mixed\n"); fail++; }
    if(gcc_split(1, 2, 3, 4, 5, 6, 7, 0.5L, 2) != 28 + 50 + 2000){ printf("FAIL gcc_split\n"); fail++; }
    long double want = ((1.5L + 1 + 0.25) * 2 + 2 + 2 + 0.5) * 2 + 0.125L + 3 + 0.75;
    if(gcc_vsum(3, 1.5L, 1, 0.25, 2.0L, 2, 0.5, 0.125L, 3, 0.75) != want){ printf("FAIL gcc_vsum\n"); fail++; }
    if(gcc_vsum(9, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0,
                1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0) != 3 * 511){ printf("FAIL gcc_vsum 9\n"); fail++; }
    struct ld1 a;
    a.x = 0.1L;
    struct ld1 ra = gcc_ld1(a);
    if(ra.x != 0.2L){ printf("FAIL gcc_ld1\n"); fail++; }
    struct ld2 b;
    b.a = 1.5L;
    b.b = -2;
    struct ld2 rb = gcc_ld2(b, 3);
    if(rb.a != 4.5L || rb.b != 1){ printf("FAIL gcc_ld2\n"); fail++; }
    struct ldk c;
    c.x = 0.75L;
    c.k = 41;
    struct ldk rc = gcc_ldk(c);
    if(rc.x != -0.75L || rc.k != 42){ printf("FAIL gcc_ldk\n"); fail++; }
    if(gcc_global != 2.75L || gcc_global_value() != 0.142857142857142857142857L){ printf("FAIL gcc_global\n"); fail++; }
    char buf[100];
    sprintf(buf, "%.4Lf %.21Lg %Le %d", 2.5L, 1.0L / 3, -1e100L, 7);
    char want_buf[100];
    sprintf(want_buf, "%.4Lf %.21Lg %Le %d", gcc_add(2, 0.5L), gcc_add(1.0L / 3, 0), gcc_add(-1e100L, 0), 7);
    if(strcmp(buf, want_buf)){ printf("FAIL printf %s %s\n", buf, want_buf); fail++; }
    return fail;
}
