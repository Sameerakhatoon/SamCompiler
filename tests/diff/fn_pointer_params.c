// Calls through function pointers whose type names the parameters:
// arguments converted to the parameter types (narrower, wider, signed
// and unsigned, struct by value), a variadic pointer to printf, a
// pointer with unspecified parameters, and pointers in parameters,
// arrays and members.
#include <stdio.h>

struct pair { int a, b; };

long widen(long x){ return x / 2; }
int narrow(signed char c){ return c; }
unsigned long low(unsigned int u){ return u; }
int sum_pair(struct pair p, short k){ return p.a + p.b * k; }
int three(int a, int b, int c){ return a * 100 + b * 10 + c; }

struct ops { int (*f)(int, int, int); };

int run(int (*g)(signed char), int x){
    return g(x);
}

int main(void){
    long (*w)(long) = widen;
    int (*n)(signed char) = narrow;
    unsigned long (*l)(unsigned int) = low;
    int (*s)(struct pair, short) = sum_pair;
    int (*v)(const char*, ...) = printf;
    int (*any)() = three;
    int (*table[1])(int, int, int) = { three };
    struct ops o = { three };
    struct pair p = { 3, 4 };
    int r = 0;
    if(w(-7) != -3) return 1;
    if(n(300) != 44) return 2;
    if(l(-1) != 4294967295ul) return 3;
    if(s(p, 65538) != 11) return 4;
    if(v("%d %s\n", 5, "x") != 4) return 5;
    if(any(1, 2, 3) != 123) return 6;
    if(table[0](4, 5, 6) != 456 || o.f(7, 8, 9) != 789) return 7;
    if(run(narrow, 257) != 1) return 8;
    r = (int)w(10) + n(-1) + (int)l(7);
    return r;
}
