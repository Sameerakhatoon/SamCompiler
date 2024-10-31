// A21: printf of floats and doubles, and sqrt from libm, checked by
// their output (tests/330-floating-point.sh).
int printf(const char* fmt, ...);
double sqrt(double x);
float sqrtf(float x);

struct sample { const char* name; double value; };

int main(void){
    struct sample s[3];
    s[0].name = "pi"; s[0].value = 3.14159265358979;
    s[1].name = "e"; s[1].value = 2.718281828459045;
    s[2].name = "tiny"; s[2].value = 1.5e-7;
    for(int i = 0; i < 3; i++){
        printf("%s %f %.3e %g\n", s[i].name, s[i].value, s[i].value, s[i].value);
    }
    double r = sqrt(2.0);
    float rf = sqrtf(9.0f);
    printf("sqrt %.10f %.1f %d\n", r, rf, (int)(r * r + 0.5));
    double acc = 0;
    for(int i = 1; i <= 10; i++){
        acc += 1.0 / i;
    }
    printf("harmonic %.6f\n", acc);
    float f = 1.0f / 3;
    printf("third %.9f %.17g\n", f, 1.0 / 3);
    printf("mix %d %.2f %c %.2f %ld %s\n", -1, -0.5, 'x', 1e3, 123456789L, "end");
    return 0;
}
