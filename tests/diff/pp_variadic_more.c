int sum3(int a, int b, int c){ return a + b * 2 + c * 3; }
#define CALL(f, ...) f(__VA_ARGS__)
#define ONLY(...) sum3(__VA_ARGS__)
int main(void){ return CALL(sum3, 1, 2, 3) + ONLY(4, 5, 6); }
