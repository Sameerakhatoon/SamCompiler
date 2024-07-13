// EXPECT: 1 2 10 11 1 7 8 8 8 2 4 30 one two 3
// enums (a trailing comma, a tag, a local one), long long and
// long int sizes, `unsigned` alone, volatile, static locals.
int printf(const char* fmt, ...);
enum {
    A_ONE = 1,
    A_TWO,
    A_TEN = 2 * 5,
    A_ELEVEN,
};
enum color { RED, GREEN };
static int div_by(unsigned long long value, unsigned base){
    value /= base;
    return (int)value;
}
static const char* name(int i){
    static const char* const x[3] = { "zero", "one", "two" };
    return x[i];
}
int counter(){ static int n; n += 1; return n; }
int main(){
    enum color c = GREEN;
    enum { LOCAL_X = 7 };
    printf("%d %d %d %d %d %d ", A_ONE, A_TWO, A_TEN, A_ELEVEN, c, LOCAL_X);
    printf("%d %d %d ", (int)sizeof(unsigned long long), (int)sizeof(long long), (int)sizeof(long int));
    printf("%d %d ", (int)sizeof(short int), (int)sizeof(unsigned));
    volatile int v = 0;
    for(volatile unsigned long n = 0; n < 3; n++){ v += 10; }
    printf("%d ", div_by(90, 3) + v - 30);
    counter(); counter();
    printf("%s %s %d\n", name(1), name(2), counter());
    return 0;
}
