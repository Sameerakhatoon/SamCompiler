int add(int a, int b){ return a + b; } int mul(int a, int b){ return a * b; } int main(void){ int (*op)(int, int) = add; int r = op(2, 3); op = mul; return r + op(2, 3); }
