/* A qualifier before a pointer typedef qualifies the pointer, not what
 * it points to: `const str s` with `typedef char* str;` is `char* const
 * s`, so `s[0] = 'x'` is valid (it was rejected as a store to a const
 * object), and `_Atomic str` is an atomic pointer
 *. */
typedef char* str;
typedef int* ip;
typedef str* strp;
typedef const char* cstr;

static int first(const str s){
    s[0] = 'q';
    return s[1];
}

int main(void){
    char a[4] = "abc";
    const str s = a;
    s[0] = 'x';
    *s = 'y';
    int v = 5;
    const ip p = &v;
    *p = 7;
    str t = a + 1;
    const strp pp = &t;
    **pp = 'z';
    *pp = a + 2;
    const cstr c = a;
    _Atomic str as = a;
    char* got = as;
    volatile str vs = a;
    vs[2] = 'w';
    str const tc = a;
    tc[3] = 0;
    int r = first(a);
    return (a[0] == 'q') + (v == 7) * 2 + (a[1] == 'z') * 4 + (*t == 'w') * 8 + (c[0] == 'q') * 16
        + (got == a) * 32 + (r == 'z') * 64 + (sizeof(as) == sizeof(char*)) * 128 - 1;
}
