// A struct a call returns, used as one of several arguments and inside
// a bigger expression. On x86 the call's result room was left under a
// second copy, which shifted the later arguments.
struct pt { int x; int y; };
struct big { int a; int b; int c; };

struct pt mk(int x, int y){ struct pt p; p.x = x; p.y = y; return p; }
struct big mkb(int a){ struct big b; b.a = a; b.b = a + 1; b.c = a + 2; return b; }
int f(struct pt a, int k, struct pt b){ return a.x * 100 + a.y * 10 + k + b.x * 7 + b.y; }
int g(struct big b, struct pt p){ return b.a + b.b * 2 + b.c * 3 + p.x - p.y; }

int main(void){
    int v = f(mk(1, 2), 3, mk(4, 5));
    int w = g(mkb(2), mk(9, 4));
    int m = mk(6, 7).y;
    int e = 1 + mk(3, 8).x * 2;
    return (v + w + m + e) % 251;
}
