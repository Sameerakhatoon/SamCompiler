int main(void){ int a[3]; a[0] = 1; a[1] = 20; a[2] = 3; int* p = &a[1]; int* q = &a[0]; return *p + *(q + 2); }
