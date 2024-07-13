// EXPECT: 9 3 12 4 1 1 5 1 0 7 2 6 300 1 20 -1 255 1 4 65 x 8 45
// expressions generated from their C types: every compound
// assignment, stores through *(p + 1) and *p++, &arr[i], unsigned
// compares and shifts, *"literal", (int)sizeof(x), *q * 100, a char
// array from a string, a local brace initializer, a call with more
// than eight arguments.
int printf(const char* fmt, ...);
int arr[4];
long sum9(long a, long b, long c, long d, long e, long f, long g, long h, long i){
    return a + b + c + d + e + f + g + h + i;
}
int main(){
    int x = 6; x += 3; printf("%d ", x);
    x -= 6; printf("%d ", x);
    x *= 4; printf("%d ", x);
    x /= 3; printf("%d ", x);
    x %= 3; printf("%d ", x);
    x &= 1; printf("%d ", x);
    x |= 4; printf("%d ", x);
    x ^= 4; printf("%d ", x);
    x <<= 3; x >>= 4; printf("%d ", x);
    int b[3]; int* p = b; *(p + 1) = 7; *p++ = 2; printf("%d %d ", b[1], b[0]);
    int* q = &arr[2]; *q = 3; printf("%d ", arr[2] * 2);
    printf("%d ", *q * 100);
    unsigned int u = 0xFFFFFFFF; printf("%d ", u > 3);
    unsigned char c = 250; c += 10; printf("%d ", c * 5);
    int m = -8; m >>= 3; printf("%d ", m);
    unsigned int big = 0xFFFFFFFF; big >>= 24; printf("%d ", big);
    printf("%d %d ", -1 < 0, (int)sizeof(x));
    printf("%d ", *"A");
    char s[4] = "x"; printf("%s ", s);
    int t[3] = {1, 2, 5}; printf("%d ", t[0] + t[2] + t[1]);
    printf("%ld\n", sum9(1, 2, 3, 4, 5, 6, 7, 8, 9));
    return 0;
}
