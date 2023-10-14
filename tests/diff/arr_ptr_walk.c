int sum(int* p, int n){ int s = 0; int i; for(i = 0; i < n; i++){ s += *p; p++; } return s; }
int main(void){ int a[4]; a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4; return sum(&a[0], 4); }
