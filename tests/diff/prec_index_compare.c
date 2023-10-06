int main(void){ int a[3]; a[0] = 1; a[1] = 5; a[2] = 9; int n = 0; if(a[1] + 1 > a[0] * 2 && a[2] - 1 == 8) n = n + 10; if(a[0] < a[1] - 4) n = n + 1; return n + (a[2] - a[1] - a[0]); }
