// EXPECT: 6 7 13 6 one=11 two=6 [a]
// function pointers as variables, parameters and struct members,
// and a table of them with a brace initializer, called by index.
int printf(const char* fmt, ...);
int puts(const char* s);
struct b { const char* name; int (*run)(int a, char** v); };
static int r1(int a, char** v);
static int r2(int a, char** v){ return a * 2; }
static const struct b tab[] = { {"one", r1}, {"two", r2} };
static int r1(int a, char** v){ return a + 8; }
int add1(int x){ return x + 1; }
int twice(int (*f)(int v), int x){ return f(f(x)); }
int each(const char* s, int (*op)(const char* p)){ return op(s); }
int main(){
    int (*op)(int x); op = add1;
    struct b one; one.run = r1;
    printf("%d %d %d %d ", op(5), twice(add1, 5), one.run(5, 0), tab[1].run(3, 0));
    for(long i = 0; i < 2; i = i + 1){ printf("%s=%d ", tab[i].name, tab[i].run(3, 0)); }
    each("[a]", puts);
    return 0;
}
