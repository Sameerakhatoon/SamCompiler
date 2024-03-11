struct pt { int x, y; char c, d; };
int main(void){ struct pt p; p.x = 1; p.y = 2; p.c = 3; p.d = 4; return p.x + p.y * 2 + p.c * 3 + p.d * 4 + sizeof(struct pt); }
