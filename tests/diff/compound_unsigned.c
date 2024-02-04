int main(void){ unsigned int u = 4000000000u; u /= 2; u >>= 28; unsigned int v = 4000000000u; v %= 7; int w = -64; w >>= 2; return u * 10 + v + w + 30; }
