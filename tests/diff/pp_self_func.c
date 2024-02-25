int f(int x){ return x + 1; }
#define f(x) f(x * 2)
int main(void){ return f(f(3)); }
