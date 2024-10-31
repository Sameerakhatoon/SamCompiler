/* gcc side of the floating-point ABI interop test (tests/331-float-interop.sh). */
#include <stdarg.h>

/* Implemented by SamCompiler. */
double sam_add(double a, double b);
float sam_mulf(float a, float b);
double sam_mixed(int a, double b, long c, float d, char e, double f);
double sam_many(double a, double b, double c, double d, double e, double f, double g, double h,
                double i, double j, float k, int l);
float sam_floats(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j);
long sam_to_long(double x);
double sam_from_int(int x, unsigned u);
double sam_calls_gcc(void);
int sam_printf_check(void);

/* Called by SamCompiler. */
double gcc_add(double a, double b) { return a + b; }
float gcc_mulf(float a, float b) { return a * b; }
double gcc_mixed(int a, double b, long c, float d, char e, double f) {
    return a + b * 10 + c * 100 + d * 1000 + e * 10000 + f * 100000;
}
double gcc_many(double a, double b, double c, double d, double e, double f, double g, double h,
                double i, double j, float k, int l) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10 + k * 11 + l * 12;
}
float gcc_floats(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}
/* A variadic callee that reads doubles: the caller must promote floats. */
double gcc_sum(int n, ...) {
    va_list ap;
    double s = 0;
    va_start(ap, n);
    for (int i = 0; i < n; i++) s += va_arg(ap, double);
    va_end(ap);
    return s;
}

int main(void) {
    int fail = 0;
    if (sam_add(1.25, 2.5) != 3.75) fail |= 1;
    if (sam_mulf(1.5f, -4.0f) != -6.0f) fail |= 2;
    if (sam_mixed(1, 2.5, 3, 0.25f, 4, 0.5) != 1 + 25 + 300 + 250 + 40000 + 50000) fail |= 4;
    if (sam_many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0.5f, 3) != 385 + 5.5 + 36) fail |= 8;
    if (sam_floats(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) != 385) fail |= 16;
    if (sam_to_long(-7.75) != -7) fail |= 32;
    if (sam_from_int(-3, 4000000000u) != 3999999997.0) fail |= 64;
    if (sam_calls_gcc() != 0) fail |= 128;
    if (sam_printf_check() != 0) fail |= 256;
    return fail ? 1 + (fail & 0x7f) + ((fail >> 7) ? 0x80 : 0) : 0;
}
