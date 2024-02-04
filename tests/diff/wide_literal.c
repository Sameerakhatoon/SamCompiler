int main(void){ long x = 5000000000; long y = 0x100000000; long z = 4000000000; return (int)(x / 1000000000) + (int)(y >> 32) * 10 + (z > 0) * 20; }
