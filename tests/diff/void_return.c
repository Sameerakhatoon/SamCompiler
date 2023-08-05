// A bare return in a void function, and a store through a pointer argument.
void f(int* p){ *p = 3; return; }
int main(void){ int x = 0; f(&x); return x; }
