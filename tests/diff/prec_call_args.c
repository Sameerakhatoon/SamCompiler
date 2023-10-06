int add(int a, int b){ return a + b; }
int main(void){ int a[2]; a[0] = 3; a[1] = 4; return add(a[0] * 2, a[1] - 1) * 3 - add(1, 2) - 2; }
