struct p { int x; int y; }; int main(void){ struct p v = (struct p){2, 3}; int* a = (int[]){5, 6}; return v.x + v.y + a[1]; }
