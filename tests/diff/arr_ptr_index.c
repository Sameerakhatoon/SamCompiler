int main(void){ int a[3]; a[0] = 4; a[1] = 5; a[2] = 6; int* p = &a[0]; int i = 2; return p[i] * 10 + p[1]; }
