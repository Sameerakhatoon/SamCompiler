int counter(void){ static int n = 10; n++; return n; } int main(void){ counter(); counter(); return counter(); }
