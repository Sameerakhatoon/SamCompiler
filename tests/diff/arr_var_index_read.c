int main(void){ int a[4]; int i; for(i = 0; i < 4; i++) a[i] = i * 3; int s = 0; for(i = 0; i < 4; i++) s = s + a[i]; int x = a[i - 1]; return s + x; }
