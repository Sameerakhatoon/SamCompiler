// EXPECT: 4830 905 9005 90 86 3 1 2
// what follows a cast or a `*p` operand belongs to the right
// operand (`a + (int)b * c`, `x - *p * 2`); a postfix operator after
// a member applies to the member (`s->n++`).
int printf(const char* fmt, ...);
struct s { int n; char buf[4]; };
int main(){
    long v = 30; unsigned b = 3;
    printf("%d ", (int)v + (int)sizeof(v) * 100 + (int)sizeof(b) * 1000);
    printf("%d ", 5 + (int)sizeof(v) * 100 + 100);
    printf("%d ", 5 + (int)sizeof(b) * 2250);
    int a[2]; int* p = a; a[0] = 5; long x = 100;
    printf("%d %d ", (int)(x - *p * 2), (int)(x - (long)*p * 3 + 1));
    struct s t; struct s* q = &t; t.n = 1;
    q->buf[q->n++] = 'x'; q->n++;
    printf("%d %d %d\n", q->n, t.buf[1] == 'x', (int)sizeof(q->buf) / 2);
    return 0;
}
