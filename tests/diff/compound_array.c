int main(void){ int a[3]; a[0] = 5; a[1] = 6; a[2] = 7; int i = 1; a[i] *= 3; a[0] -= 2; a[i + 1] <<= 2; return a[0] + a[1] + a[2]; }
