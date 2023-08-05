int sq(int x){ return x * x; }
int add(int a, int b){ return a + b; }
int main(void){ return add(sq(2), add(sq(3), 1)); }
