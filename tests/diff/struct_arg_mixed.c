// B1: structs among scalars, so the registers run out in the middle
// of one: x86_64 puts the whole struct on the stack and keeps r9 for
// the next scalar, aarch64 puts it on the stack and stops using
// registers, riscv64 splits it between a7 and the stack.
struct s8  { int a, b; };
struct s12 { int a, b, c; };
struct s16 { long a, b; };
struct s24 { long a, b, c; };
struct s40 { long a, b, c, d, e; };

long late5(long r1, long r2, long r3, long r4, long r5, struct s16 s, long r6){
    return r1 + 2 * r2 + 3 * r3 + 4 * r4 + 5 * r5 + 6 * s.a + 7 * s.b + 8 * r6;
}

long late7(long r1, long r2, long r3, long r4, long r5, long r6, long r7, struct s16 s, long r8){
    return r1 + 2 * r2 + 3 * r3 + 4 * r4 + 5 * r5 + 6 * r6 + 7 * r7 + 8 * s.a + 9 * s.b + 10 * r8;
}

long late7_12(int r1, int r2, int r3, int r4, int r5, int r6, int r7, struct s12 s, char r8){
    return r1 + 2 * r2 + 3 * r3 + 4 * r4 + 5 * r5 + 6 * r6 + 7 * r7 + 8 * s.a + 9 * s.b + 10 * s.c + 11 * r8;
}

long many(struct s12 a, long b, struct s24 c, long d, long e, struct s16 f, long g, long h,
          struct s8 i, long j, struct s40 k, int l){
    return a.a + a.b + a.c + 2 * b + 3 * (c.a + c.b + c.c) + 4 * d + 5 * e + 6 * (f.a + f.b)
        + 7 * g + 8 * h + 9 * (i.a + i.b) + 10 * j + 11 * (k.a + k.b + k.c + k.d + k.e) + 12 * l;
}

long last_reg(long r1, long r2, long r3, long r4, long r5, long r6, long r7, struct s8 s){
    return r1 + r2 + r3 + r4 + r5 + r6 + r7 + 3 * s.a + 5 * s.b;
}

int main(void){
    struct s8 i = { 31, 32 };
    struct s12 a = { 1, 2, 3 };
    struct s16 f = { 17, 18 };
    struct s24 c = { 4, 5, 6 };
    struct s40 k = { 41, 42, 43, 44, 45 };
    long sum = late5(1, 2, 3, 4, 5, f, 6);
    sum = sum * 3 + late7(1, 2, 3, 4, 5, 6, 7, f, 8);
    sum = sum * 3 + late7_12(1, 2, 3, 4, 5, 6, 7, a, 9);
    sum = sum * 3 + many(a, 7, c, 8, 9, f, 10, 11, i, 12, k, 13);
    sum = sum * 3 + last_reg(1, 2, 3, 4, 5, 6, 7, i);
    return (int)(sum % 127);
}
