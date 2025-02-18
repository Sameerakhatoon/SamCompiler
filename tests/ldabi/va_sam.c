// variadic functions built by SamCompiler that read structs
// of long doubles with va_arg; tests/ldabi/va_gcc.c (built by gcc)
// calls them. On aarch64 a struct of one to four long doubles is a
// homogeneous aggregate in q registers, else on the stack.
#include <stdarg.h>

struct q1 { long double a; };
struct q2 { long double a, b; };
struct q3 { long double a, b, c; };
struct q4 { long double a, b, c, d; };
struct qi { long double a; int n; };

long double sam_q(int n, ...){
    va_list ap;
    va_start(ap, n);
    long double s = 0;
    for(int i = 0; i < n; i++){
        struct q1 a = va_arg(ap, struct q1);
        struct q2 b = va_arg(ap, struct q2);
        double d = va_arg(ap, double);
        struct q3 c = va_arg(ap, struct q3);
        long double x = va_arg(ap, long double);
        s = s * 3 + a.a + 2 * b.a + 3 * b.b + d + 5 * c.a + 7 * c.b + 11 * c.c + 13 * x;
    }
    va_end(ap);
    return s;
}

long double sam_q4(int n, ...){
    va_list ap;
    va_start(ap, n);
    long double s = 0;
    for(int i = 0; i < n; i++){
        struct q4 q = va_arg(ap, struct q4);
        int k = va_arg(ap, int);
        s = s * 2 + q.a - q.b + 3 * q.c - 5 * q.d + k;
    }
    va_end(ap);
    return s;
}

// Eight ints first: the last one goes to the stack on every target, so
// the structs after the two that take all the q registers on aarch64
// are 16-byte aligned on the stack past an 8-byte word.
long double sam_qx(int n, ...){
    va_list ap;
    va_start(ap, n);
    long double s = n;
    for(int i = 0; i < 7; i++){
        s = s * 2 + va_arg(ap, int);
    }
    struct q4 a = va_arg(ap, struct q4);
    struct q4 b = va_arg(ap, struct q4);
    struct q1 c = va_arg(ap, struct q1);
    int k = va_arg(ap, int);
    struct q2 d = va_arg(ap, struct q2);
    long double x = va_arg(ap, long double);
    struct q3 e = va_arg(ap, struct q3);
    va_end(ap);
    return s + a.a + 2 * a.d + 3 * b.b + 5 * b.c + 7 * c.a + 11 * k + 13 * d.a + 17 * d.b + 19 * x + 23 * e.c;
}

long double sam_qi(int n, ...){
    va_list ap;
    va_start(ap, n);
    long double s = 0;
    for(int i = 0; i < n; i++){
        struct qi q = va_arg(ap, struct qi);
        struct q2 p = va_arg(ap, struct q2);
        s = s * 2 + q.a * q.n + p.a / p.b;
    }
    va_end(ap);
    return s;
}

// The other way: these call gcc's copies of the functions above (the
// renamed copy of this file calls the same ones), so the structs are
// passed by SamCompiler to gcc-built code.
long double gcc_q(int n, ...);
long double gcc_qx(int n, ...);
long double gcc_qi(int n, ...);

long double sam_call(int k){
    struct q1 a = { 1.25L }, a2 = { -7.5L };
    struct q2 b = { 2.5L, 1.0L / 3 }, b2 = { 11, 0.125L };
    struct q3 c = { 3, 4, 5 }, c2 = { -1.0L / 7, 9, 1e30L };
    struct q4 d = { 1, 2, 3, 4 }, d2 = { 0.5L, -0.25L, 8, 16 };
    struct qi e = { 1.5L, 3 }, e2 = { -2.25L, -7 };
    if(k == 0){
        return gcc_q(2, a, b, 0.5, c, 6.0L, a2, b2, -3.0, c2, 1.0L / 3);
    }
    if(k == 1){
        return gcc_qx(1, 2, 3, 4, 5, 6, 7, 8, d, d2, a2, 9, b, 1.0L / 3, c2);
    }
    return gcc_qi(3, e, b, e2, b2, e, b);
}
