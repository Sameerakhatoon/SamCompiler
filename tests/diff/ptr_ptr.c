int main(void){ int x = 6; int* p = &x; int** pp = &p; **pp = 9; return **pp + *p + x; }
