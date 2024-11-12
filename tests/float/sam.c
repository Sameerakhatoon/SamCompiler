// SamCompiler side of the floating-point ABI interop test
// (tests/331-float-interop.sh); gcc.c is the gcc side.
#include <stdarg.h>
int printf(const char* fmt, ...);
int sprintf(char* out, const char* fmt, ...);
int strcmp(const char* a, const char* b);

double gcc_add(double a, double b);
float gcc_mulf(float a, float b);
double gcc_mixed(int a, double b, long c, float d, char e, double f);
double gcc_many(double a, double b, double c, double d, double e, double f, double g, double h,
                double i, double j, float k, int l);
float gcc_floats(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j);
double gcc_sum(int n, ...);

double sam_add(double a, double b){ return a + b; }
float sam_mulf(float a, float b){ return a * b; }
double sam_mixed(int a, double b, long c, float d, char e, double f){
    return a + b * 10 + c * 100 + d * 1000 + e * 10000 + f * 100000;
}
double sam_many(double a, double b, double c, double d, double e, double f, double g, double h,
                double i, double j, float k, int l){
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10 + k * 11 + l * 12;
}
float sam_floats(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j){
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}
long sam_to_long(double x){ return x; }

// A variadic callee called by gcc, reading doubles between longs: more
// doubles than floating-point argument registers, so the last ones are
// on the stack (x86_64, aarch64), in the integer registers and on the
// stack (riscv64), or 8 bytes each on the stack (x86).
double sam_vsum(int n, ...){
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for(int i = 0; i < n; i++){
        s = s * 2 + va_arg(ap, double);
        s = s + va_arg(ap, long);
    }
    va_end(ap);
    return s;
}
double sam_from_int(int x, unsigned u){ return x + (double)u; }

double sam_calls_gcc(void){
    int fail = 0;
    float half = 0.5f;
    if(gcc_add(1.25, 2.5) != 3.75) fail = fail | 1;
    if(gcc_mulf(1.5f, -4) != -6.0f) fail = fail | 2;
    if(gcc_mixed(1, 2.5, 3, 0.25f, 4, 0.5) != 1 + 25 + 300 + 250 + 40000 + 50000) fail = fail | 4;
    if(gcc_many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, half, 3) != 385 + 5.5 + 36) fail = fail | 8;
    if(gcc_floats(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) != 385) fail = fail | 16;
    if(gcc_sum(3, 1.5, half, (double)2) != 4) fail = fail | 32;
    if(gcc_sum(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0) != 55) fail = fail | 64;
    return fail;
}

// printf of doubles and of floats (promoted), next to ints, more than
// eight of them so some go on the stack.
int sam_printf_check(void){
    char buf[200];
    float f = 2.5f;
    sprintf(buf, "%d %.2f %d %.1f %g %e %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d", 1, 3.25, 2, f, 0.1 + 0.2, 1234.5,
            1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9);
    printf("%s\n", buf);
    return strcmp(buf, "1 3.25 2 2.5 0.3 1.234500e+03 1.000 2.000 3.000 4.000 5.000 6.000 7.000 8.000 9");
}
