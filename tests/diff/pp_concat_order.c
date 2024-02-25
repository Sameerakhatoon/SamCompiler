#define CAT(a, b) 10 + a##b
int xy = 5;
int main(void){ return CAT(x, y); }
