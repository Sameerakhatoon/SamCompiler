struct s { int a; int b; };
int main(void){ struct s v; v.a = 10; v.b = 3; v.a -= v.b; v.b *= 4; struct s* p = &v; p->a /= 2; p->b %= 5; return v.a * 10 + v.b; }
