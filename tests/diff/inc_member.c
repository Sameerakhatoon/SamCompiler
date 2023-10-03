struct s { int a; char b; };
int main(void){ struct s v; v.a = 1; v.b = 2; v.a++; ++v.b; struct s* p = &v; p->a++; p->b += 3; return v.a * 10 + v.b; }
