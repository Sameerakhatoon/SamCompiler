int main(void){ int a[4], b, *p; b = 7; p = &b; a[3] = 2; return sizeof(int) + *p + a[3] * 10; }
