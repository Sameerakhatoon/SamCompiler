int main(void){ int a[5]; int* p = &a[1]; int* q = &a[4]; long n = q - p; return (int)n * 10 + (q > p) + (p < q) * 2 + (p == q) * 4 + (p != q) * 8; }
