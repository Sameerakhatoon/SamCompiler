int f(int a, int b){ return a || b; }
int g(int a, int b){ return b && !a; }
int main(void){ return f(0,3)*10 + g(0,1)*20 + f(0,0) + g(1,1)*40; }
