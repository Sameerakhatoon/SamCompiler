static inline int f(register int a){ volatile int b = a; return b; }
int main(void){ return f(4); }
