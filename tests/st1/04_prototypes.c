// EXPECT: 5 ok
// a static prototype before the definition, `struct x;` before
// `struct x { ... }`, a parameter pointing to a struct only declared.
int printf(const char* fmt, ...);
struct later;
int takes(struct later* p);
static int f(int a);
struct later { int v; };
static int f(int a){ return a + 1; }
int takes(struct later* p){ return p->v; }
int main(){ struct later l; l.v = 0; printf("%d %s\n", f(4) + takes(&l), "ok"); return 0; }
