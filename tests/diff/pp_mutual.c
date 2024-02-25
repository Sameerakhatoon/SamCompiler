int a = 4, b = 9;
#define a b
#define b a
int main(void){ return a * 10 + b; }
