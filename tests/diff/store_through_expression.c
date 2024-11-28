// Stores through a pointer expression: `*(p + 1) = v`, `*p++ = v`,
// compound assignment and chars. On x86 the stored object had no type,
// and the store was written `mov (null) [edx], eax`.
int main(void){
    int a[4] = {1, 2, 3, 4};
    int* p = a;
    *(p + 1) = 7;
    *p++ = 9;
    *(p + 1) += 5;
    *p++ += 1;
    char s[4] = "abc";
    char* c = s;
    *(c + 2) = 'z';
    *c++ = 'y';
    *(a + 3) -= 2;
    return a[0] + a[1] * 2 + a[2] * 3 + a[3] * 4 + (int)(p - a) + (s[0] == 'y') + (s[2] == 'z') + (int)(c - s);
}
