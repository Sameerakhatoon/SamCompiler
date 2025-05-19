/* _Alignas and <stdalign.h>'s alignas, with an integer constant and with
 * a type, on globals, statics, locals (at several stack depths), struct
 * and union members and nested structs: the addresses, offsetof,
 * sizeof and _Alignof against gcc on all four targets
 *. _Alignas inside a function made the function
 * vanish and the variable a global. */
#include <stdalign.h>
#include <stddef.h>

static unsigned h;
static void mix(unsigned long v){ h = h * 31 + (unsigned)v; }
static int aligned(const void* p, unsigned long a){ return ((unsigned long)p % a) == 0; }

char gpad0;
_Alignas(16) char g1[3];
char gpad1;
alignas(32) int g2 = 5;
char gpad2;
_Alignas(double) char g3[5] = "abcd";
char gpad3;
static _Alignas(64) short g4;
char gpad4;
_Alignas(16) _Thread_local int tl = 7;

struct m { char c; _Alignas(16) char d; int x; };
struct n { char c; struct m inner; };
struct p { char c; alignas(long) char l; };
union u { char c; _Alignas(8) char d[3]; };
struct m gm;
char gpad5;
struct n gn = { 1, { 2, 3, 4 } };
char gpad6;
union u gu;

static int leaf(int depth){
    char a = 1;
    _Alignas(16) char buf[5];
    short b = 2;
    alignas(8) char c8;
    _Alignas(16) struct p lp;
    struct m lm;
    char z;
    int ok = aligned(buf, 16) + aligned(&c8, 8) + aligned(&lp, 16) + aligned(&lm, 16) * 2 + aligned(&lm.d, 16) * 4;
    buf[0] = a; c8 = (char)b; z = 0; lm.x = 1; lp.l = 3;
    ok += buf[0] + c8 + z + lm.x + lp.l;
    if(depth){
        char odd[3] = { 1, 2, 3 };
        ok += leaf(depth - 1) + odd[depth % 3];
    }
    return ok;
}

int main(void){
    mix(aligned(g1, 16)); mix(aligned(&g2, 32)); mix(aligned(g3, _Alignof(double))); mix(aligned(&g4, 64));
    mix(aligned(&tl, 16)); mix(tl);
    mix(sizeof(struct m)); mix(_Alignof(struct m)); mix(offsetof(struct m, d)); mix(offsetof(struct m, x));
    mix(sizeof(struct n)); mix(_Alignof(struct n)); mix(offsetof(struct n, inner));
    mix(sizeof(struct p)); mix(_Alignof(struct p)); mix(offsetof(struct p, l));
    mix(sizeof(union u)); mix(_Alignof(union u));
    mix(aligned(&gm, 16)); mix(aligned(&gn, 16)); mix(aligned(&gn.inner.d, 16)); mix(aligned(&gu, 8));
    mix(gn.inner.d + gn.inner.x);
    mix(__alignof__(g1)); mix(__alignof__(g2)); mix(__alignof__(gm.d)); mix(__alignof__(gm.c));
    mix(g2); mix(g3[2]);
    mix(leaf(3));
    for(int i = 0; i < 3; i++){
        char pad = (char)i;
        _Alignas(16) double d[2] = { 1.5, 2.5 };
        mix(aligned(d, 16)); mix((int)(d[1] * 2) + pad);
    }
    _Alignas(0) int zero = 3;               mix(zero);
    static _Alignas(128) char sbuf[2];      mix(aligned(sbuf, 128)); sbuf[1] = 4; mix(sbuf[1]);
    char c1 = 1;
    _Alignas(16) char a1 = 2, a2 = 3;       mix(aligned(&a1, 16) + aligned(&a2, 16) + a1 + a2 + c1);
    _Alignas(int) char ci = 4;              mix(aligned(&ci, _Alignof(int)) + ci);
    char _Alignas(16) after = 5;            mix(aligned(&after, 16) + after);
    struct m lm = { 1, 2, 3 };              mix(aligned(&lm, 16) + lm.d + lm.x);
    return (int)(h % 127);
}
