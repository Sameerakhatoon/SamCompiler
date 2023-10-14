struct p { int x; int y; };
int main(void){ struct p a[3]; int i; for(i = 0; i < 3; i++){ a[i].x = i; a[i].y = i * 2; } return a[2].y * 10 + a[1].x + a[i - 1].x; }
