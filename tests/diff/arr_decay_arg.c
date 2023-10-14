int sum(int* p, int n){ int s = 0; int i; for(i = 0; i < n; i++) s = s + p[i]; return s; }
int main(void){ int a[3]; a[0] = 1; a[1] = 2; a[2] = 3; return sum(a, 3); }
