// _Atomic structs, unions and long doubles: the lock-free
// sizes (1, 2, 4, 8 bytes) through the target's instructions, the others
// through libatomic; loads, stores, exchange and compare-exchange by
// assignment, <stdatomic.h> and the generic builtins, and the layout
// gcc gives them. The exit status is a checksum of the output.
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>

static unsigned long checksum = 5381;

// The line printed and added to the checksum the exit status is made
// of (under 128: the differential reads 128 and up as a signal).
static void emit(const char* line){
    fputs(line, stdout);
    for(const char* p = line; *p; p++){
        checksum = checksum * 33 + (unsigned char)*p;
    }
}

#define report(...) do { char line[256]; snprintf(line, sizeof(line), __VA_ARGS__); emit(line); } while(0)

struct s2 { char a, b; };
struct s3 { char a[3]; };
struct s4 { short a, b; };
struct s8 { int a, b; };
struct s16 { int a, b, c, d; };
struct s24 { int a; int b[5]; };
union u8 { int i[2]; char c[8]; };

struct holder {
    char tag;
    _Atomic struct s8 pair;
    char tail;
    _Atomic struct s16 quad;
};

// The alignments, as offsets after a char.
struct a2 { char c; _Atomic struct s2 x; };
struct a3 { char c; _Atomic struct s3 x; };
struct a4 { char c; _Atomic struct s4 x; };
struct a8 { char c; _Atomic struct s8 x; };
struct a16 { char c; _Atomic struct s16 x; };
struct au8 { char c; _Atomic union u8 x; };
struct ald { char c; _Atomic long double x; };
struct adbl { char c; _Atomic double x; };

_Atomic struct s8 g8 = { 1, 2 };
_Atomic struct s24 g24;
_Atomic long double gld = 2.5L;
_Atomic struct s4 arr[3];

static int eq8(struct s8 x, int a, int b){
    return x.a == a && x.b == b;
}

static void layout(void){
    report("sizes %d %d %d %d %d %d %d\n", (int)sizeof(_Atomic struct s2), (int)sizeof(_Atomic struct s3),
           (int)sizeof(_Atomic struct s4), (int)sizeof(_Atomic struct s8), (int)sizeof(_Atomic struct s16),
           (int)sizeof(_Atomic struct s24), (int)sizeof(_Atomic union u8));
    report("aligns %d %d %d %d %d %d %d %d\n", (int)offsetof(struct a2, x), (int)offsetof(struct a3, x),
           (int)offsetof(struct a4, x), (int)offsetof(struct a8, x), (int)offsetof(struct a16, x),
           (int)offsetof(struct au8, x), (int)offsetof(struct ald, x), (int)offsetof(struct adbl, x));
    report("holder %d %d %d %d\n", (int)sizeof(struct holder), (int)offsetof(struct holder, pair),
           (int)offsetof(struct holder, tail), (int)offsetof(struct holder, quad));
    report("long double %d\n", (int)sizeof(_Atomic long double));
    _Atomic struct s8 l8;
    char c = 0;
    _Atomic struct s4 l4;
    report("aligned %d %d %d %d %d\n", (int)((unsigned long)&g8 % 8), (int)((unsigned long)&arr[1] % 4),
           (int)((unsigned long)&l8 % 8), (int)((unsigned long)&l4 % 4), c);
}

static void assign(void){
    _Atomic struct s8 a8 = { 3, 4 };
    struct s8 v = a8;
    report("s8 load %d %d\n", v.a, v.b);
    a8 = (struct s8){ 5, 6 };
    v = a8;
    report("s8 store %d %d\n", v.a, v.b);
    g8 = a8;
    report("g8 %d\n", eq8(g8, 5, 6));

    _Atomic struct s3 a3;
    struct s3 t3 = { { 'x', 'y', 'z' } };
    a3 = t3;
    struct s3 r3 = a3;
    report("s3 %c%c%c\n", r3.a[0], r3.a[1], r3.a[2]);

    _Atomic struct s16 a16;
    a16 = (struct s16){ 10, 20, 30, 40 };
    struct s16 r16 = a16;
    report("s16 %d %d %d %d\n", r16.a, r16.b, r16.c, r16.d);

    struct s24 t24 = { 1234567890, { 1, 2, 3, 4 } };
    g24 = t24;
    struct s24 r24 = g24;
    report("s24 %d %d %d\n", r24.a, r24.b[0], r24.b[3]);

    arr[1] = (struct s4){ 7, 8 };
    struct s4 r4 = arr[1];
    report("arr %d %d\n", r4.a, r4.b);

    _Atomic union u8 au;
    union u8 tu;
    tu.i[0] = 0x01020304;
    tu.i[1] = -1;
    au = tu;
    union u8 ru = au;
    report("u8 %x %d\n", ru.i[0], ru.i[1]);

    _Atomic struct s2 a2 = { 'p', 'q' };
    struct s2 r2 = a2;
    report("s2 %c%c\n", r2.a, r2.b);
}

static void stdatomic(void){
    _Atomic struct s8 a8;
    atomic_init(&a8, ((struct s8){ 1, 1 }));
    atomic_store(&a8, ((struct s8){ 2, 3 }));
    struct s8 v = atomic_load(&a8);
    report("std load %d %d\n", v.a, v.b);
    struct s8 old = atomic_exchange(&a8, ((struct s8){ 4, 5 }));
    report("std exchange %d %d -> %d\n", old.a, old.b, eq8(atomic_load(&a8), 4, 5));
    struct s8 expected = { 0, 0 };
    int ok = atomic_compare_exchange_strong(&a8, &expected, ((struct s8){ 6, 7 }));
    report("std cas fail %d %d %d %d\n", ok, expected.a, expected.b, eq8(atomic_load(&a8), 4, 5));
    ok = atomic_compare_exchange_strong(&a8, &expected, ((struct s8){ 6, 7 }));
    report("std cas ok %d %d\n", ok, eq8(atomic_load(&a8), 6, 7));
    v = atomic_load_explicit(&a8, memory_order_acquire);
    atomic_store_explicit(&a8, v, memory_order_release);

    _Atomic struct s24 a24;
    struct s24 t = { 5, { 6, 7, 8, 9 } };
    atomic_store(&a24, t);
    struct s24 r = atomic_load(&a24);
    report("std s24 %d %d\n", r.a, r.b[2]);
    struct s24 e = { 5, { 6, 7, 8, 0 } };
    ok = atomic_compare_exchange_weak(&a24, &e, t);
    report("std s24 cas %d %d\n", ok, e.b[3]);
    t.a = 99;
    ok = atomic_compare_exchange_strong_explicit(&a24, &e, t, memory_order_acq_rel, memory_order_acquire);
    r = atomic_exchange(&a24, e);
    struct s24 now = atomic_load(&a24);
    report("std s24 cas %d %d %d\n", ok, r.a, now.a);
}

static void generic(void){
    _Atomic struct s16 a;
    struct s16 v = { 1, 2, 3, 4 }, r, e;
    __atomic_store(&a, &v, __ATOMIC_SEQ_CST);
    __atomic_load(&a, &r, __ATOMIC_RELAXED);
    report("gen load %d %d\n", r.a, r.d);
    v.d = 40;
    __atomic_exchange(&a, &v, &r, __ATOMIC_ACQ_REL);
    __atomic_load(&a, &e, __ATOMIC_ACQUIRE);
    report("gen exchange %d %d\n", r.d, e.d);
    e.d = 0;
    int ok = __atomic_compare_exchange(&a, &e, &v, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
    report("gen cas %d %d\n", ok, e.d);
    v.a = -1;
    ok = __atomic_compare_exchange(&a, &e, &v, 1, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    __atomic_load(&a, &r, __ATOMIC_SEQ_CST);
    report("gen cas %d %d\n", ok, r.a);

    struct s4 p = { 3, 9 }, q;
    __atomic_store(&arr[2], &p, __ATOMIC_RELEASE);
    __atomic_load(&arr[2], &q, __ATOMIC_ACQUIRE);
    report("gen s4 %d %d\n", q.a, q.b);
}

static void ldouble(void){
    long double x = gld;
    report("ld load %g\n", (double)x);
    gld = 1.25L;
    gld = 3;
    report("ld store %g\n", (double)gld);
    gld += 0.5;
    gld *= 2;
    ++gld;
    long double before = gld--;
    report("ld update %g %g\n", (double)before, (double)gld);
    long double old = atomic_exchange(&gld, 10.0L);
    report("ld exchange %g %g\n", (double)old, (double)atomic_load(&gld));
    // x86_64: the 6 padding bytes count in the compare, and gcc leaves
    // them as they come, so the first try may fail; the second has the
    // object's own bytes in e.
    long double e = 10.0L;
    int tries = 0;
    while(!atomic_compare_exchange_strong(&gld, &e, 11.0L) && tries < 5){
        tries++;
    }
    report("ld cas %d %g %g\n", tries < 2, (double)e, (double)gld);
    _Atomic long double local = -0.5L;
    local -= 1;
    atomic_store(&local, local * 4);
    report("ld local %g\n", (double)atomic_load(&local));
}

static void lock_free(void){
    _Atomic struct s8 a8;
    _Atomic struct s4 a4;
    _Atomic struct s16 a16;
    report("lock free %d %d %d\n", (int)atomic_is_lock_free(&a8), (int)atomic_is_lock_free(&a4), (int)atomic_is_lock_free(&a16) >= 0);
    report("always %d %d %d %d\n", __atomic_always_lock_free(8, 0), __atomic_always_lock_free(16, 0),
           __atomic_always_lock_free(3, 0), __atomic_always_lock_free(4, (void*)2));
    report("macros %d %d %d %d\n", ATOMIC_INT_LOCK_FREE, ATOMIC_LLONG_LOCK_FREE, ATOMIC_POINTER_LOCK_FREE, ATOMIC_CHAR_LOCK_FREE);
}

int main(void){
    layout();
    assign();
    stdatomic();
    generic();
    ldouble();
    lock_free();
    return (int)((checksum >> 16) % 127);
}
