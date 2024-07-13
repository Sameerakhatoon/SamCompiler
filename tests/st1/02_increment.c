// EXPECT: 4 34 47 44 bc 2
// ++ and --, prefix and postfix, on ints, array elements and
// pointers, as statements and as values.
int printf(const char* fmt, ...);
int main(){
    int n = 3; n++;
    int a = 3; int m = a++;
    char b[5]; int k = 3; b[k++] = 7;
    int c = 5; int d = --c;
    char* s = "abc"; char* p = s; p++;
    char x = *p++;
    printf("%d %d %d %d %c%c %d\n", n, m * 10 + a, b[3] + k * 10, d * 10 + c, x, *p, (int)(p - s));
    return 0;
}
