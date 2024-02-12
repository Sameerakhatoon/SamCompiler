int f(int a);
int f(int);
int g(void){ return f(2); }
int f(int a){ return a * 3; }
int main(void){ return g() + f(1); }
