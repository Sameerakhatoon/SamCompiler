int main(void){ int a; int b = (a = 4) + 1; signed char c; int d = (c = 300); return a + b * 10 + (d == 44) * 60; }
