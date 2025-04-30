/* setjmp and longjmp, whose jmp_buf is an array of one struct: on i386
 * an array argument was counted as its element's bytes (a jmp_buf's
 * struct, a double), not the pointer it decays to, and the call failed
 * with "call arguments do not match their stack size"
 *. */
#include <setjmp.h>

static jmp_buf env;
static int depth;

struct t { int a[6]; int m; };
typedef struct t tb[1];
struct t g[1];

static int f(struct t* p, int v){ return p->m + v; }
static int h(tb p, int v){ return p[0].m + v; }
static double sum(double* d, int n){ double s = 0; for(int i = 0; i < n; i++) s += d[i]; return s; }

static void dive(int n){
    if(n > 0) dive(n - 1);
    else longjmp(env, 7);
}

int main(void){
    int r = setjmp(env);
    if(r < 3) longjmp(env, r + 1);
    volatile int tries = 0;
    int s = setjmp(env);
    tries++;
    if(s == 0) dive(5);
    depth = s;
    g[0].m = 4;
    tb l;
    l[0].m = 5;
    double d[3] = { 1.5, 2.5, 3 };
    double m[2][2] = { { 1, 2 }, { 3, 4 } };
    int calls = f(g, 1) + h(g, 2) + f(l, 3) + h(l, 4) + (int)sum(d, 3) + (int)sum(m[1], 2);
    return r * 10 + depth + tries + calls;
}
