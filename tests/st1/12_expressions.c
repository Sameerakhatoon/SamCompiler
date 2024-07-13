// EXPECT: 11 111 1 95 [ x] 1 13600 3 3 2 7 11 1
// a cast takes a unary operand (`(int)a + b`); a comma ends an
// initializer and a ternary's false branch; && and || chains with
// calls in them; a call, subscript or member left of an operator.
int printf(const char* fmt, ...);
int snprintf(char* b, unsigned long n, const char* f, ...);
int f(int a){ return a; }
int g2(int a, int b){ return a * 10 + b; }
int main(){
    long s = 3725;
    char out[16]; int i = 1; int from = 0;
    long a = 300; char* p = (char*)0 + 5; int b = (int)a + 2; char c = (char)a;
    snprintf(out, 16, i > from ? " %s" : "%s", "x");
    long t = 100000; long days = t / 86400, secs = t % 86400;
    int x1 = 0; int y1 = 1; int z1 = 0;
    int v[3]; v[1] = 5;
    printf("%d %d %d %d [%s] %d %d ", g2((int)(s % 7), 1), g2((int)(s / 60 % 7), 1) + 50, (int)s % 7,
        (int)((b + (long)p + c) & 255), out, days, secs);
    printf("%d %d %d %d %d %d\n", (f(x1) < 0 || f(z1) != 1) ? 3 : 4,
        (x1 < 0 || f(y1) < 0 || z1 >= 9 || f(z1) != 1) ? 3 : 4,
        (y1 && f(1) && !z1) + 1, f(2) * 3 + 1, v[i] * 2 + 1, f(1) == 1);
    return 0;
}
