/* A conditional whose operands are struct (or long double) values from
 * calls: only one branch runs, so the stack after it must hold one
 * result. Both branches' rooms were released, the stack was off by a
 * struct and misaligned for the printf that followed.
 *
 * The struct cases are for the 64-bit targets: the i386 backend still
 * miscompiles a struct-valued conditional. */
#include <stdio.h>

struct small { long a, b, c; };
struct big { long w[11]; };

static struct small make_small(long base){
    struct small s = { base, base + 1, base + 2 };
    return s;
}

static struct big make_big(long base){
    struct big b;
    for(int i = 0; i < 11; i++){
        b.w[i] = base * 100 + i;
    }
    return b;
}

static long double half(long double x){
    return x / 2;
}

static long sum_big(struct big b){
    long total = 0;
    for(int i = 0; i < 11; i++){
        total += b.w[i];
    }
    return total;
}

int main(void){
    long check = 0;
    for(int i = 0; i < 4; i++){
#ifdef __LP64__
        struct small s = i & 1 ? make_small(i) : make_small(10 * i);
        struct big b = i < 2 ? make_big(i) : make_big(i + 7);
        // Calls with a misaligned stack fault in glibc's printf.
        printf("%d: %ld %ld %ld / %ld %ld\n", i, s.a, s.b, s.c, b.w[0], b.w[10]);
        check += s.a + s.c + sum_big(b);
        struct small n = i == 0 ? make_small(1) : i == 1 ? make_small(2) : make_small(3);
        check += n.b;
        struct small m = i > 1 ? make_small(i) : make_small(-i);
        check += m.c;
        check += sum_big(i ? make_big(i) : make_big(5));
#else
        struct small s = make_small(i & 1 ? i : 10 * i);
        struct big b = make_big(i < 2 ? i : i + 7);
        printf("%d: %ld %ld %ld / %ld %ld\n", i, s.a, s.b, s.c, b.w[0], b.w[10]);
        check += s.a + s.c + sum_big(b);
        check += i == 0 ? 2 : i == 1 ? 3 : 4;
        check += i > 1 ? i + 2 : 2 - i;
        check += sum_big(make_big(i ? i : 5));
#endif
        long double h = i ? half(i) : half(9);
        printf("%.3f\n", (double)h);
        check += (long)(h * 4);
    }
    printf("check %ld\n", check);
    return (int)(check & 0x7f);
}
