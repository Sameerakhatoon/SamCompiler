int main(void){ unsigned char a = 200; unsigned char b = 100; int s = a + b; signed char c = -100; int d = c / 3; return (s == 300) + (d == -33) * 2 + ((a << 1) == 400) * 4; }
