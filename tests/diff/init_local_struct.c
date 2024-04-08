struct p { int x; char c; long l; }; int main(void){ struct p v = {3, 4, 5}; return v.x + v.c + v.l; }
