// Statements wrapped in parentheses. On x86 a parenthesized statement
// generated no code at all, so `(b = a);` left b unchanged.
struct pt { int x; int y; };
int g;
int calls;
int bump(void){ calls++; return calls; }

int main(void){
    int a = 3;
    int b = 0;
    (b = a);
    ((b) = (b + 1));
    (g = 5);
    (g += 2);
    (a++);
    ((a)--);
    (++a);
    struct pt p;
    struct pt* q = &p;
    (q->x = 10);
    (q->y = 20);
    (bump());
    ((bump()));
    return (b * 100 + g * 10 + a + p.x + p.y + calls) % 127;
}
