/* long double parameters, results and variadic arguments
   between SamCompiler functions and to printf: more of them than there
   are registers (q registers on aarch64, integer pairs on riscv64, one
   left in a7 for a split), mixed with doubles and ints, through a
   function pointer, in a struct by value, and read with va_arg. */
#include <stdarg.h>
int snprintf(char* out, unsigned long n, const char* fmt, ...);
int strcmp(const char* a, const char* b);

long double add(long double a, long double b){ return a + b; }
long double scale(int k, long double x, double d){ return k * x + d; }
long double many(long double a, long double b, long double c, long double d, long double e,
                 long double f, long double g, long double h, long double i, long double j){
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}
/* Seven longs take a0-a6 on riscv64: the long double is split between
   a7 and the stack. */
long double split(long a, long b, long c, long d, long e, long f, long g, long double x, int k){
    return a + b + c + d + e + f + g + x * 100 + k * 1000;
}
long double mixed(double a, long double b, float c, long double d, int e, long double f){
    return a + b * 10 + c * 100 + d * 1000 + e * 10000 + f * 100000;
}
long double fact(int n){ return n <= 1 ? 1 : n * fact(n - 1); }
int to_int(long double x){ return x; }
double to_double(long double x){ return x; }

struct ldpair { long double x; int k; };
struct ldpair make(long double x, int k){ struct ldpair p; p.x = x; p.k = k; return p; }
long double take(struct ldpair p){ return p.x * p.k; }

long double vsum(int n, ...){
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

int main(void){
    int fail = 0;
    if(add(1.5L, 0.25L) != 1.75L || add(1, 2) != 3) fail |= 1;
    if(scale(3, 0.5L, 0.25) != 1.75L) fail |= 2;
    if(many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) != 385) fail |= 4;
    if(split(1, 2, 3, 4, 5, 6, 7, 0.5L, 2) != 28 + 50 + 2000) fail |= 8;
    if(mixed(1, 2, 3, 4, 5, 6) != 1 + 20 + 300 + 4000 + 50000 + 600000) fail |= 16;
    if(fact(20) != 2432902008176640000.0L || fact(25) / fact(24) != 25) fail |= 32;
    if(to_int(-3.75L) != -3 || to_double(0.5L) != 0.5) fail |= 64;
    long double (*f)(long double, long double) = add;
    if(f(0.5L, f(1, 1)) != 2.5L) fail |= 128;
    struct ldpair p = make(1.25L, 4);
    if(p.x != 1.25L || p.k != 4 || take(p) != 5 || take(make(0.5L, 3)) != 1.5L) fail |= 256;
    long double s = vsum(3, 1.5L, 1, 0.25, 2.0L, 2, 0.5, 0.125L, 3, 0.75);
    if(s != ((1.5L + 1 + 0.25) * 2 + 2 + 2 + 0.5) * 2 + 0.125L + 3 + 0.75) fail |= 512;
    long double t = vsum(9, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0,
                         1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0, 1.0L, 1, 1.0);
    if(t != 3 * 511) fail |= 1024;
    char buf[100];
    snprintf(buf, sizeof buf, "%.3Lf %Lg %.2Le %d", 2.5L, add(0.125L, 1), -1e100L, 7);
    if(strcmp(buf, "2.500 1.125 -1.00e+100 7")) fail |= 2048;
    return fail % 251 + (fail != 0);
}
