// typedefs of function types (`typedef int binop(int, int);`,
// as glibc's <bits/types/cookie_io_functions_t.h> has), used through
// pointers, as struct members, parameters and in other typedefs, and
// pointer typedefs with several declarators (`P a, b;` makes both
// pointers).
#include <stdio.h>

typedef int binop(int, int);
typedef binop* binop_ptr;
typedef long reader(void* cookie, char* buf, unsigned long size);

static int add(int a, int b){ return a + b; }
static int mul(int a, int b){ return a * b; }
static long read_some(void* cookie, char* buf, unsigned long size){
    buf[0] = 'x';
    return (long)size + *(int*)cookie;
}

struct ops { binop *first, *second; binop_ptr third; reader *read; };

static int apply(binop *f, int a, int b){ return f(a, b); }

typedef int* int_ptr;
typedef int_ptr* int_ptr_ptr;
typedef struct ops* ops_ptr;
struct holder { int_ptr a, b; ops_ptr o, *op; };
int_ptr global_a, global_b;

int main(void){
    binop *p = add;
    binop_ptr q = mul, r = add;
    struct ops o = { add, mul, add, read_some };
    int cookie = 3;
    char buf[4];
    printf("%d %d %d %d %d %d\n", p(1, 2), q(3, 4), r(5, 6), o.first(7, 8), o.second(2, 9), apply(mul, 6, 7));
    printf("%ld %c %d\n", o.read(&cookie, buf, 10), buf[0], (int)sizeof(binop*));

    int x = 5, y = 6;
    int_ptr a = &x, b = &y, *pb = &b;
    int_ptr_ptr pp = &a, pq = &b;
    ops_ptr u = &o, w = &o;
    struct holder h = { &x, &y, &o, &u };
    global_a = &x;
    global_b = &y;
    printf("%d %d %d %d %d %d\n", *a, *b, **pb, **pp + **pq, u->third(1, 1) + w->first(2, 2), (int)sizeof(b));
    printf("%d %d %d %d %d\n", *h.a, *h.b, h.o->second(3, 3), (*h.op)->first(4, 4), *global_a + *global_b);
    return 0;
}
