// EXPECT: 1 1 0 1 2
// `defined(A) || defined(B)` was defined(A) alone: the
// parentheses after defined took `|| defined(B)` with them.
int printf(const char* fmt, ...);
#define B 1
int main(){
    int a = 0, b = 0, c = 0, d = 0, e = 0;
#if defined(NOT_DEFINED) || defined(B)
    a = 1;
#endif
#if defined(NOT_DEFINED) || 1
    b = 1;
#endif
#if defined(B) && defined(NOT_DEFINED)
    c = 1;
#endif
#if defined(B) && !defined(NOT_DEFINED)
    d = 1;
#endif
#if defined(NOT_DEFINED) || defined(OTHER) || defined(B)
    e = 2;
#endif
    printf("%d %d %d %d %d\n", a, b, c, d, e);
    return 0;
}
