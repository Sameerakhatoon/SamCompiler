// long double arithmetic in a global's initializer, folded at
// compile time in the target's format (x87 on x86 and x86_64, binary128
// on aarch64 and riscv64), against the same operations done at run time
// on volatile operands. The exit status also hashes the bytes, so a
// wrong rounding on both sides still differs from gcc.
#include <string.h>

long double third = 1.0L / 3;
long double two_thirds = 2.0L / 3;
static long double t[] = { 2.0L * 3, -1.5L + 1, 1.0L - 1.0L, -0.0L - 0.0L, 0.1L + 0.2L, 0.1L * 3 };
long double mixed[] = { 0.1 + 0.2L, 3 * 1.0L / 7, (long double)1 / 49 * 49, 2147483647 * 3.0L, 1 - 1e-200L, 1 + 1e-200L };
long double tiny[] = { 1e-4000L * 1e-900L, 1e-4940L / 3, 1e300L * 1e300L * 1e300L, -(1.0L / 3) * 3 };
struct pair { int n; long double x; } pair = { 7, 10.0L / 4 - 0.125L };

long double one_l = 1.0L;

// The bytes that hold the value: 10 in the x87 format (the explicit
// integer bit of 1.0 is in byte 7), the 16 of binary128.
int value_bytes(void){
    unsigned char b[16];
    memcpy(b, &one_l, 8);
    return b[7] ? 10 : 16;
}

int same(long double a, long double b){
    return memcmp(&a, &b, value_bytes()) == 0;
}

int check(void){
    volatile long double one = 1, two = 2, three = 3, tenth = 0.1L, fifth = 0.2L, z = 0;
    volatile long double big = 1e300L, small = 1e-200L, seven = 7, half = 1.5L;
    int bad = 0;
    bad += !same(third, one / three);
    bad += !same(two_thirds, two / three);
    bad += !same(t[0], two * three);
    bad += !same(t[1], -half + one);
    bad += !same(t[2], one - one);
    bad += !same(t[3], -z - z);
    bad += !same(t[4], tenth + fifth);
    bad += !same(t[5], tenth * three);
    bad += !same(mixed[0], (long double)0.1 + fifth);
    bad += !same(mixed[1], three * one / seven);
    bad += !same(mixed[3], 2147483647 * three);
    bad += !same(mixed[4], one - small);
    bad += !same(mixed[5], one + small);
    bad += !same(tiny[2], big * big * big);
    bad += !same(tiny[3], -(one / three) * three);
    bad += !same(pair.x, (two * 5) / (two * two) - 0.125L);
    return bad;
}

int main(void){
    static long double local[] = { 5.0L / 9, -2.0L / 7 };
    unsigned hash = 0;
    long double* all[] = { &third, &two_thirds, &t[0], &t[1], &t[2], &t[3], &t[4], &t[5],
        &mixed[0], &mixed[1], &mixed[2], &mixed[3], &mixed[4], &mixed[5],
        &tiny[0], &tiny[1], &tiny[2], &tiny[3], &pair.x, &local[0], &local[1] };
    for(int i = 0; i < 21; i++){
        unsigned char b[16];
        memcpy(b, all[i], value_bytes());
        for(int k = 0; k < value_bytes(); k++){
            hash = hash * 31 + b[k];
        }
    }
    int bad = check();
    if(bad){
        return 200 + bad;
    }
    return (hash ^ (hash >> 8) ^ (hash >> 16) ^ (hash >> 24)) & 127;
}
