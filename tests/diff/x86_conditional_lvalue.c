/* Stores through a conditional: a member or element of `c ? p : q` as
 * the left operand of =, op= and ++. On i386 the resolver
 * has no entity for a conditional: `(c ? p : q)->m = 7` crashed the
 * compiler (exit status 1 and no message), `*(c ? a : b) = 7`
 * assembled a `mov (null)`, and `(&x)->m` used x's contents as the
 * pointer. */

struct in { short s; char t; };
struct S { int m; char c; double d; int a[3]; struct in in; float f; };

static unsigned h;
static void mix(unsigned long v){ h = h * 31 + (unsigned)v; }

static void dump(struct S* s){
    mix(s->m); mix(s->c); mix((unsigned long)(s->d * 4)); mix(s->a[0]); mix(s->a[1]); mix(s->a[2]);
    mix(s->in.s); mix(s->in.t); mix((unsigned long)(s->f * 2));
}

static int stores(int c, struct S* p, struct S* q){
    (c ? p : q)->m = 7;
    (c ? p : q)->c = 3;
    (c ? p : q)->d = 2.5;
    (c ? p : q)->a[1] = 4;
    (c ? p : q)->a[2] += 5;
    (c ? p : q)->m++;
    ++(c ? p : q)->in.s;
    (c ? p : q)->in.s *= 3;
    (c ? p : q)->in.t -= 2;
    (c ? p : q)->d += 1.25;
    (c ? p : q)->f = 1.5f;
    (c ? &p->in : &q->in)->t += 10;
    int v = ((c ? p : q)->a[0] = 20) + 1;
    int w = (c ? p : q)->m--;
    return v + w;
}

/* A member through an address, `(&x)->m`, read and written: on i386 it
 * went through x's contents as if they were a pointer. */
static int through_address(struct S* p){
    struct S local = *p;
    (&p->in)->t = 33;
    (&local)->m = 9;
    (&local)->a[1] += 2;
    ((&local.in))->s = 4;
    (&(&local)->in)->t = 6;
    return (&p->in)->t + (&local)->m + local.a[1] + (&local.in)->s + local.in.t + (*&p)->m;
}

int main(void){
    struct S s = { 1, 1, 1.0, { 1, 1, 1 }, { 1, 1 }, 0.5f };
    struct S t = { 2, 2, 2.0, { 2, 2, 2 }, { 2, 2 }, 1.0f };
    for(int c = 0; c < 2; c++){
        mix(stores(c, &s, &t));
        dump(&s); dump(&t);
    }
    int a1[3] = { 0, 0, 0 }, a2[3] = { 0, 0, 0 };
    for(int c = 0; c < 2; c++){
        (c ? a1 : a2)[c + 1] = 9 + c;
        *(c ? a1 : a2) = 5 + c;
        *(c ? &a1[2] : &a2[2]) += 3;
        int* p = a1;
        int* q = a2;
        *(c ? p : q) += 1;
        (*(c ? p + 1 : q + 1))++;
    }
    for(int i = 0; i < 3; i++){ mix(a1[i]); mix(a2[i]); }
    mix(through_address(&s)); dump(&s);
    char x = 1, y = 2;
    *(x > y ? &x : &y) = 40;
    mix(x); mix(y);
    double dx = 0.5, dy = 1.5;
    *(dx < dy ? &dx : &dy) *= 4;
    mix((unsigned long)(dx * 10)); mix((unsigned long)(dy * 10));
    return (int)(h % 127);
}
