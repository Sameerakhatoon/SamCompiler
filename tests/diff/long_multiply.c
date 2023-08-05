// long is 64-bit on the LP64 targets and 32-bit on x86; gcc agrees on both.
int main(void){ long a = 100000; long b = a * a; return (int)(b / 1000000000); }
