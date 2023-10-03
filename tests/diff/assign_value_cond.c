int f(int* p){ return *p; }
int main(void){ int x; int n = 0; while((x = n) < 5){ n++; } int y; if((y = 3) == 3){ return x + y + f(&x); } return 0; }
