#define SUM(a, ...) (a + __VA_ARGS__)
int main(void){ return SUM(1, 2); }
