int main(void){ unsigned int a = 0; a = a - 1; unsigned int b = a / 2; return (a == 4294967295u) + (b == 2147483647u) * 2 + (a > 0) * 4; }
