// the # operator. The string was the text the lexer recorded
// after the macro's (, so it took the ) of an enclosing call
// (`strlen(STR(abc))` was 4), stopped at a nested ( (`STR(SQUARE(2))`
// was "SQUARE("), and a stringized macro expansion (`XSTR(TWO)`)
// crashed the compiler. Returns the number of the first string that
// differs, 0 when all match.
#define STR(x) #x
#define XSTR(x) STR(x)
#define SQUARE(x) ((x) * (x))
#define TWO 2
#define E a + b
#define EMPTY
#define HEX 0x10
#define VSTR(...) #__VA_ARGS__
#define CAT(a, b) a ## b
#define PAIR(a, b) #a "|" #b

static unsigned long length(const char *s)
{
    unsigned long n = 0;
    while (s[n])
        n++;
    return n;
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static int first_bad;
static int checked;

static void check(const char *got, const char *want)
{
    checked++;
    if (!first_bad && !same(got, want))
        first_bad = checked;
}

int main(void)
{
    check(STR(abc), "abc");
    check(length(STR(abc)) == 3 ? "y" : "n", "y");
    check(STR(SQUARE(2)), "SQUARE(2)");
    check(XSTR(TWO), "2");
    check(XSTR(SQUARE(2)), "((2) * (2))");
    check(XSTR(SQUARE(TWO)), "((2) * (2))");
    check(STR(f()), "f()");
    check(STR((a, b)), "(a, b)");
    check(STR(  a   +    b  ), "a + b");
    check(STR(a/* c */b), "a b");
    check(STR("hi\n" 'x' '\\' "q\"z"), "\"hi\\n\" 'x' '\\\\' \"q\\\"z\"");
    check(STR(L"w" u'c'), "L\"w\" u'c'");
    check(XSTR(E c), "a + b c");
    check(XSTR(a EMPTY b), "a b");
    check(XSTR(HEX), "0x10");
    check(STR(0x1fUL 1.5f 1e+5 .5 010), "0x1fUL 1.5f 1e+5 .5 010");
    check(VSTR(a, b,c ,  d), "a, b,c , d");
    check(VSTR(), "");
    check(STR(), "");
    check(PAIR(x y, (1,2)), "x y|(1,2)");
    check(XSTR(CAT(TW, O)), "2");
    check(XSTR(STR(q)), "\"q\"");
    check(STR(a
              b), "a b");
    check(STR(f(x)[y]->z.w), "f(x)[y]->z.w");
    check(STR(a+=b<<=c), "a+=b<<=c");
    check(XSTR(__LINE__) [0] != 0 ? "y" : "n", "y");
    return first_bad;
}
