int main(void){ signed char c = 127; int old = c++; unsigned char u = 255; int o2 = u++; return (old == 127) + (c == -128) * 2 + (o2 == 255) * 4 + (u == 0) * 8; }
