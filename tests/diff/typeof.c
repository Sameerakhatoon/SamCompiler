/* typeof, __typeof__ and __typeof of expressions and type names, in
 * local and global declarations, a declarator list, a typedef, casts,
 * sizeof and a struct member. typeof inside a
 * function made the whole function vanish without an error. */
#include <stddef.h>

struct pt { int x, y; };
typedef unsigned short u16;
static int g = 3;
typeof(g) g2 = 4;
__typeof__(int*) gp = &g;
typedef typeof(g) gint;
typedef __typeof__(struct pt*) pt_ptr;
struct holder { typeof(g) a; __typeof__(char[3]) b; typeof(u16) c; };

static int twice(int v){ return v * 2; }
static int sum3(int a, int b, int c){ return a + b + c; }
static typeof(g) add(typeof(g) a, __typeof__(int*) b, typeof(struct pt)* c){ return a + *b + c->y; }

static unsigned h;
static void mix(unsigned long v){ h = h * 31 + (unsigned)v; }

int main(void){
    int x = 5;
    typeof(x) y = x + 1;                    mix(y);
    typeof(int*) p = &x, q = &y;            mix(*p + *q);
    __typeof__(x) arr[3] = { 1, 2, 3 };     mix(arr[2]);
    __typeof(arr) arr2 = { 4 };             mix(sizeof(arr2) / sizeof(arr2[0])); mix(arr2[0]);
    typeof(struct pt) s = { 7, 8 };         mix(s.y);
    typeof(s)* sp = &s;                     mix(sp->x);
    pt_ptr sp2 = sp;                        mix(sp2->y);
    mix(add(1, &x, sp));
    typeof(u16) us = 65535; us++;           mix(us);
    typeof('a') c = 'b';                    mix(sizeof(c));
    typeof(x + 1L) l = 1;                   mix(sizeof(l) == sizeof(long)); mix(l);
    typeof(twice(2)) t = twice(3);          mix(t);
    typeof(&twice) fp = twice;              mix(fp(5));
    typeof(twice)* fp2 = twice;             mix(fp2(6));
    typeof(sum3)* f3 = sum3;                mix(f3(1, 2, 3));
    int (*fps[2])(int) = { twice, twice };
    typeof(fps) fps2; fps2[0] = twice;      mix(fps2[0](1)); mix(sizeof fps2 / sizeof fps2[0]);
    mix((typeof(x))3.9);
    mix(sizeof(typeof(double)));
    mix(sizeof(typeof(arr)));
    mix(sizeof(__typeof__(struct pt)));
    const typeof(x) cx = 9;                 mix(cx);
    unsigned char uc = 200; typeof(uc) uc2 = uc + 100; mix(uc2);
    char ch = -1; typeof(ch) ch2 = ch;      mix(ch2 < 0);
    signed char sc = -2; typeof(sc) sc2 = sc; mix(sc2 < 0);
    gint gi = 2;                            mix(gi + g2 + *gp);
    typeof(typeof(x)*) pp = &x;             mix(*pp);
    long double ld = 1.5L; typeof(ld) ld2 = ld * 2; mix((int)ld2); mix(sizeof(ld2) == sizeof(long double));
    float fl = 2.5f; typeof(fl * 2) fl2 = fl * 2; mix(sizeof(fl2)); mix((int)fl2);
    double db = 0.25; typeof(db + fl) db2 = 3.75; mix(sizeof(db2)); mix((int)(db2 * 4));
    _Bool b = 1; typeof(b) b2 = 5;          mix(b2);
    unsigned long ul = 0; typeof(ul - 1) ul2 = ul - 1; mix(ul2 > 0);
    typeof(*sp) s2 = *sp;                   mix(s2.x + s2.y);
    typeof(s.y) member = 11;                mix(member);
    typeof(arr[0]) element = 12;            mix(element);
    struct holder hd = { 1, { 2, 3, 4 }, 5 };
    mix(sizeof hd); mix(hd.a + hd.b[2] + hd.c);
    typeof(sizeof(int)) sz = 0; sz--;       mix(sz > 0); mix(sizeof sz);
    for(typeof(x) i = 0; i < 3; i++){ mix(i); }
    {
        typeof(x) x = 42;                   mix(x);
    }
    void* vp = &x; typeof(vp) vp2 = vp;     mix(*(int*)vp2);
    char* const cp = 0; typeof(cp) cp2 = cp; mix(cp2 == 0);
    return (int)(h % 127);
}
