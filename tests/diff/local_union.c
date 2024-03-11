union u { char c; int i; };
int main(void){ union u x; x.i = 0; x.c = 5; return x.i + sizeof(union u); }
