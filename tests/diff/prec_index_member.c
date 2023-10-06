struct s { int a; int b; };
int main(void){ struct s arr[2]; arr[1].a = 5; arr[1].b = 3; arr[0].a = 2; return arr[1].a * 2 + arr[1].b - arr[0].a * 3 + 1; }
