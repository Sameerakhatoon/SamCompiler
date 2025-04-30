/* A compound literal in a controlling expression is a new object each
 * time the expression is evaluated (C11 6.5.2.5p16). In a loop condition
 * or a for step its initialization ran once, before the loop, and in any
 * controlling expression the hidden declaration went inside the body,
 * so `while(!f((struct s){x, y}))` was rejected as using an undeclared
 * `__compound_0`. */
struct s { int a, b; };

static int f(struct s v){ return v.a + v.b > 10; }
static int sum(struct s v){ return v.a + v.b; }
static int g(struct s* p){ return p->a * p->b; }
static int calls;
static int bump(void){ return ++calls; }

int main(void){
    int x = 0, y = 1, n = 0;
    while(!f((struct s){ x, y })){ x++; y++; n++; }
    int t = 0;
    for(int i = 0; i < 4; i++) t += g(&(struct s){ i, i + 1 });
    int k = 0;
    do { k++; } while(((struct s){ k, 0 }).a < 5);
    int total = 0;
    for(int i = 0; i < 3; i++){ int* p = (int[]){ i, i * 2 }; total += p[1]; }
    int m = 0;
    for(int i = 0; sum((struct s){ i, m }) < 20; i += ((struct s){ 1, 2 }).b) m++;
    int r = 0, z = 3;
    if(sum((struct s){ z, 4 }) == 7) r += 1;
    if(z > 5) r += 100;
    else if(sum((struct s){ z, bump() }) == 4) r += 2;
    switch(sum((struct s){ 1, z })){ case 4: r += 4; break; default: r += 50; }
    int q = 0;
    while(q < 3 && g(&(struct s){ q, 2 }) < 100) q++;
    return n + t + k + total + m + r * 3 + calls + q;
}
