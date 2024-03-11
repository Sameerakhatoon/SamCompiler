typedef struct { int a; int b; } T;
typedef int myint;
int main(void){ T t; t.a = 3; t.b = 4; myint m = 2; return t.a * t.b + m; }
