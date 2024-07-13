// EXPECT: 10 32 6 8 9 24
// sizeof of an expression, constant array sizes, and
// `(sizeof(x) / 2)`, which is not a cast.
int printf(const char* fmt, ...);
#define N 4
char* tab[4];
struct st { int a; char b[6]; };
int main(){
    char buf[10]; struct st v; char c[N * 2 + 1];
    long e = sizeof(buf); long f = sizeof(tab); long g = sizeof(v.b); long h = sizeof(tab[0]);
    long i = sizeof(c); long j = (sizeof(tab) / sizeof(tab[0])) * 6;
    printf("%d %d %d %d %d %d\n", e, f, g, h, i, j);
    return 0;
}
