int main(void){ int a[3]; a[0] = 5; a[1] = 6; a[2] = 7; int* p = a; int* q; q = a; return *p + *(q + 2); }
