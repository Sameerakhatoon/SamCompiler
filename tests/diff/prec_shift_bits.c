int main(void){ int a[2]; a[0] = 3; a[1] = 12; return (a[0] << 2 | a[1] >> 2) + (a[1] & 6 ^ 1); }
