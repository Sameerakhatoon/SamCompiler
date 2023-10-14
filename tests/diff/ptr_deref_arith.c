int main(void){ int x = 3; int y = 4; int* p = &x; int* q = &y; char c = 5; char* pc = &c; return *p + *q * 2 + 10 + *pc + 1; }
