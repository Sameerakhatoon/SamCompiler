int apply(int (*f)(int), int x){ return f(x); } int sq(int x){ return x * x; } int main(void){ return apply(sq, 5); }
