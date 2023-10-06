struct s { int a; int b; };
int main(void){ struct s v; v.a = 7; v.b = 2; struct s* p = &v; return v.a * 2 + v.b - 1 + p->a - p->b * 3 + v.a % v.b; }
