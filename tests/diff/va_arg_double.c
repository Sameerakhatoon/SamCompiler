// va_arg of a double, mixed with ints, longs, pointers and structs.
// x86_64 and aarch64 pass a variadic double in the next floating-point
// register (xmm0-7, v0-7) and on the stack once those are used up,
// with the integer arguments counted apart; riscv64 passes it in the
// integer registers; i386 pushes it as 8 bytes. Named doubles before
// the `...` use up floating-point registers too, and a float passed to
// `...` arrives as a double.
#include <stdarg.h>

struct s16 { long a, b; };

double sum(int n, ...){
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for(int i = 0; i < n; i++){
        s = s * 2 + va_arg(ap, double);
    }
    va_end(ap);
    return s;
}

// Alternating kinds: "d" double, "i" int, "l" long, "p" pointer to
// int, "s" struct s16.
double mixed(const char* kinds, ...){
    va_list ap;
    va_start(ap, kinds);
    double total = 0;
    for(const char* k = kinds; *k; k++){
        if(*k == 'd'){
            total = total * 3 + va_arg(ap, double);
        } else if(*k == 'i'){
            total = total * 3 + va_arg(ap, int);
        } else if(*k == 'l'){
            total = total * 3 + va_arg(ap, long);
        } else if(*k == 'p'){
            total = total * 3 + *va_arg(ap, int*);
        } else {
            struct s16 s = va_arg(ap, struct s16);
            total = total * 3 + s.a - s.b;
        }
    }
    va_end(ap);
    return total;
}

// Named doubles and ints before the variadic ones.
double named(double a, int b, double c, double d, double e, double f, double g, int n, ...){
    va_list ap;
    va_start(ap, n);
    double total = a + b + c + d + e + f + g;
    for(int i = 0; i < n; i++){
        total = total * 2 + va_arg(ap, double);
        total = total - va_arg(ap, int);
    }
    va_end(ap);
    return total;
}

// A va_list handed on, and copied.
double take_two(va_list* ap){
    double x = va_arg(*ap, double);
    return x * 10 + va_arg(*ap, double);
}

double copied(int n, ...){
    va_list ap, again;
    va_start(ap, n);
    va_copy(again, ap);
    double first = take_two(&ap);
    double second = take_two(&again);
    va_end(again);
    va_end(ap);
    return first + second * 100 + n;
}

int main(void){
    if(sum(3, 1.5, 2.25, -4.0) != ((1.5 * 2) + 2.25) * 2 - 4.0) return 1;
    double expect = 0;
    double values[12] = { 1, 2.5, 3, 4.5, 5, 6.5, 7, 8.5, 9, 10.5, 11, -12.25 };
    for(int i = 0; i < 12; i++){
        expect = expect * 2 + values[i];
    }
    if(sum(12, 1.0, 2.5, 3.0, 4.5, 5.0, 6.5, 7.0, 8.5, 9.0, 10.5, 11.0, -12.25) != expect) return 2;
    float f = 0.75f;
    if(sum(2, f, 2.0) != 0.75 * 2 + 2.0) return 3;
    int seven = 7;
    struct s16 s = { 9, 4 };
    double m = mixed("dildpsdddddddddii", 0.5, 2, 3L, 1.25, &seven, s, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.5, 10, 11);
    double e = 0;
    e = e * 3 + 0.5; e = e * 3 + 2; e = e * 3 + 3; e = e * 3 + 1.25; e = e * 3 + 7; e = e * 3 + 5;
    for(int i = 1; i <= 8; i++){
        e = e * 3 + i;
    }
    e = e * 3 + 9.5; e = e * 3 + 10; e = e * 3 + 11;
    if(m != e) return 4;
    double n = named(1.5, 2, 3.5, 4.5, 5.5, 6.5, 7.5, 3, 8.5, 1, 9.5, 2, 10.5, 3);
    double ne = 1.5 + 2 + 3.5 + 4.5 + 5.5 + 6.5 + 7.5;
    ne = ne * 2 + 8.5 - 1; ne = ne * 2 + 9.5 - 2; ne = ne * 2 + 10.5 - 3;
    if(n != ne) return 5;
    if(copied(4, 1.5, 2.5) != 17.5 + 1750 + 4) return 6;
    return 0;
}
