/* Struct-valued conditionals in every place a struct value goes: an
 * initializer, an assignment, a call argument, a return value, a member
 * of one, nested ones, with operands that are calls, variables, `*p`
 * and members, members of one with a double, a pointer or an array
 * operand. The i386 backend kept one result per branch on the
 * stack; the loops make an
 * unbalanced stack show. */
#include <stdio.h>

struct pt { int x, y; };
struct tri { int a, b, c; };
struct odd { char c[5]; };
struct wrap { int tag; struct tri t; };
struct d { int k; double v; };
struct in { struct tri t; int arr[3]; };

static struct tri mk(int base){
    struct tri t = { base, base + 1, base + 2 };
    return t;
}

static struct pt mkp(int x){
    struct pt p = { x, -x };
    return p;
}

static int sum(struct tri t){
    return t.a + t.b + t.c;
}

static int sum2(int k, struct tri t, int m){
    return k * 1000 + t.a * 100 + t.b * 10 + t.c + m;
}

static struct tri pick(int c, struct tri u, struct tri v){
    return c ? u : v;
}

static struct tri pick_call(int c){
    return c > 1 ? mk(c) : mk(-c);
}

static struct d mkd(int k){
    struct d r = { k, k * 1.5 };
    return r;
}

static double scale(double f, struct tri t){
    return f * (t.a + t.b + t.c);
}

static int odd_sum(struct odd o){
    return o.c[0] + o.c[4];
}

int main(void){
    long check = 0;
    struct tri g = { 7, 8, 9 };
    struct tri h = { 70, 80, 90 };
    struct tri* p = &h;
    struct wrap w = { 1, { 4, 5, 6 } };
    struct odd o1 = { { 1, 2, 3, 4, 5 } };
    struct odd o2 = { { 10, 20, 30, 40, 50 } };
    struct d ds = { 1, 2.5 }, dt = { 2, 4.5 };
    struct tri* r = &g;
    int ia[3] = { 1, 2, 3 }, ib[3] = { 4, 5, 6 };
    struct in n1 = { { 1, 2, 3 }, { 4, 5, 6 } }, n2 = { { 10, 20, 30 }, { 40, 50, 60 } };
    for(int i = 0; i < 6; i++){
        struct tri s = i & 1 ? mk(i) : mk(10 * i);
        check += s.a + s.c;
        s = i < 3 ? g : *p;
        check += s.b;
        s = i == 0 ? mk(1) : i == 1 ? g : i == 2 ? *p : w.t;
        check += s.a * 3 + s.c;
        check += sum(i ? mk(i) : g);
        check += sum2(i, i > 2 ? *p : mk(i), i + 1);
        check += (i & 2 ? g : h).b;
        check += (i ? mk(i) : mk(50)).c;
        check += (i > 3 ? w.t : *p).a;
        struct pt q = i % 3 ? mkp(i) : mkp(i * 7);
        check += q.x - q.y;
        check += pick(i & 1, g, h).c;
        check += pick_call(i).a;
        check += odd_sum(i & 1 ? o1 : o2);
        struct odd oo = i > 2 ? o1 : o2;
        check += oo.c[2];
        // A struct whose size is not a multiple of 4 is stored as its own
        // bytes, not over the variable next to it.
        check += q.x;
        check += (long)((i & 1 ? ds : dt).v * 2);
        struct d e = i > 1 ? mkd(i) : ds;
        check += e.k + (long)(e.v * 2);
        check += (i > 1 ? mkd(i) : dt).k;
        check += (long)scale(0.5, i ? mk(i) : g);
        check += (i & 1 ? r : p)->b;
        check += (i & 2 ? ia : ib)[i % 3];
        check += (i ? n1 : n2).t.c;
        check += (i > 2 ? n1 : n2).arr[i % 3];
        check += (i & 1 ? mk(i) : g).a + 1;
        printf("%d: %d %d %d / %d %d / %ld\n", i, s.a, s.b, s.c, q.x, q.y, check);
    }
    printf("check %ld\n", check);
    return (int)(check & 0x7f);
}
