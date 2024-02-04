struct s { int x; int y; int z; };
int main(void){ struct s a[6]; struct s* p = &a[1]; struct s* q = &a[5]; return (int)(q - p) + (int)(p - q) + 10; }
