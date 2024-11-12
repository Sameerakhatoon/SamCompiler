// Calls through function pointers whose prototypes have float and
// double parameters: the arguments are converted to the parameter
// types (an int to a float, a double to a float, a float to a double),
// a function returning such a pointer, and a variadic one that reads
// doubles.
#include <stdarg.h>

float scale(float x, double y){ return x * 2 + (float)y; }
double mix(int a, float b, double c, long d){ return a + b * 10 + c * 100 + d * 1000; }

double vsum(int n, ...){
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for(int i = 0; i < n; i++){
        s = s * 2 + va_arg(ap, double);
    }
    va_end(ap);
    return s;
}

float (*pick(int which))(float, double){
    return which ? scale : 0;
}

struct ops { double (*m)(int, float, double, long); double (*v)(int, ...); };

int main(void){
    float (*f)(float, double) = scale;
    if(f(3, 1) != 7.0f) return 1;
    if(f(1.25, 0.5f) != 3.0f) return 2;
    struct ops o = { mix, vsum };
    if(o.m(1.5, 2, 3.25f, 4) != 1 + 20 + 325 + 4000) return 3;
    float half = 0.5f;
    if(o.v(3, 1.0, half, 2.0) != (1.0 * 2 + 0.5) * 2 + 2.0) return 4;
    if(pick(1)(2.5, 1.5) != 6.5f) return 5;
    double (*pv)(int, ...) = vsum;
    if(pv(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0) != 2036) return 6;
    return 0;
}
