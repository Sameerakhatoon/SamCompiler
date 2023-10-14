struct s { int x; int* p; };
int main(void){ int a = 7; struct s v; struct s* w = &v; w->p = &a; *w->p = 9; int* q = w->p; return *w->p + *q + a; }
