int main(void){ long a = 3000000000; long b = a * 3; long c = b - 9000000000; long d = -a / 7; return (int)c + (d < 0) * 10 + (int)(b % 1000) / 100; }
