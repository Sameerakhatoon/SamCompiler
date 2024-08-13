struct p { int x; int y; }; int main(void){ struct p a = {1, 2}; struct p b; struct p* q = &b; *q = a; return b.x + b.y; }
