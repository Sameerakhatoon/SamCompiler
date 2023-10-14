int main(void){ int a = 1; int b = 2; int* v[2]; v[0] = &a; v[1] = &b; int i = 1; return *v[0] + *v[i] * 10; }
