int f(void){ return 4; }
int g(int x){ return x + 1; }
int main(void){ return f() * 2 + 1 - f() - 1 + g(2) * 3 - g(1) % 2; }
