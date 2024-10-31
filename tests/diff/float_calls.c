/* A21: float and double parameters and results between SamCompiler
   functions: more than the eight floating-point argument registers,
   mixed with integers, through a function pointer, recursion, and a
   struct with floating members by value. */
struct pair { double a; float b; };

double add(double a, double b){ return a + b; }
float halve(float x){ return x / 2; }
double from_int(int i){ return i; }          /* converted on return */
int to_int(double d){ return d; }
double many(double a, int i, double b, float c, double d, double e, double f, double g,
            double h, double j, double k, float l, int m){
    return a + i + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + j * 9 + k * 10 + l * 11 + m;
}
double power(double x, int n){ return n == 0 ? 1 : x * power(x, n - 1); }
struct pair make(double a, float b){ struct pair p; p.a = a; p.b = b; return p; }
double take(struct pair p, double scale){ return (p.a + p.b) * scale; }
double apply(double (*f)(double, double), double x){ return f(x, x); }

int main(void){
    int fail = 0;
    if(add(1.25, 2) != 3.25) fail |= 1;
    if(halve(3) != 1.5f) fail |= 2;
    if(from_int(-4) != -4.0 || to_int(-4.5) != -4) fail |= 4;
    if(many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13) != 1 + 2 + 6 + 12 + 20 + 30 + 42 + 56 + 72 + 90 + 110 + 132 + 13) fail |= 8;
    if(power(1.5, 4) != 5.0625) fail |= 16;
    struct pair p = make(1.5, 0.25f);
    if(p.a != 1.5 || p.b != 0.25f) fail |= 32;
    if(take(p, 4) != 7) fail |= 64;
    if(apply(add, 1.75) != 3.5) fail |= 128;
    return fail % 255;
}
