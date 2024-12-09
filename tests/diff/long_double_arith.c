/* long double arithmetic, comparisons and conversions: x87
   80-bit on x86 and x86_64, IEEE binary128 on aarch64 and riscv64. The
   checks are on values whose answers differ from double's where the
   precision does, so they hold for gcc's long double on each target. */
long double g = 1.25L;
long double gneg = -0.375L;
long double garr[4] = { 0.5L, -2, 3.0, 1e-3L };
long double gzero;
long double ghex = 0x1.8p-3L;
long double gint = 7;
long double gbig = 1e310L;
long double gtiny = 1e-310L;
long double gdiv = 0.25;

int arith(void){
    int fail = 0;
    long double a = 1.5L, b = 0.1L;
    if(a + b != 1.6L) fail |= 1;
    if(a - b != 1.4L) fail |= 2;
    if(a * 4 != 6 || a * b == 0.15) fail |= 4;
    if(a / 3 != 0.5L) fail |= 8;
    /* More precision than a double: 0.1L is not 0.1. */
    if(b - 0.1 == 0) fail |= 16;
    if((double)b != 0.1) fail |= 32;
    long double third = 1.0L / 3;
    if(third * 3 != 1) fail |= 64;
    long double big = 9007199254740993.0L;      /* 2^53 + 1: no double has it */
    if(big - 9007199254740992.0L != 1) fail |= 128;
    if(-a != -1.5L || -(-a) != a) fail |= 256;
    long double z = 0;
    if(-z != 0) fail |= 512;
    return fail;
}

int compare(void){
    int fail = 0;
    long double a = 2.5L, b = 2.5L + 1e-15L, c = 2.5L;
    int r = (a < b) + (a <= c) * 2 + (b > a) * 4 + (a >= c) * 8 + (a == c) * 16 + (a != b) * 32;
    if(r != 63) fail |= 1;
    long double zero = 0;
    long double nan = zero / zero;
    if(nan == nan || nan < 1 || nan > 1 || nan <= 1 || nan >= 1 || !(nan != nan)) fail |= 2;
    if(!nan) fail |= 4;
    if(zero) fail |= 8;
    long double negzero = -zero;
    if(negzero != 0 || (negzero ? 1 : 0)) fail |= 16;
    long double x = 0;
    int n = 0;
    while(x < 10){
        x += 0.5L;
        n++;
    }
    if(n != 20) fail |= 32;
    for(long double y = 1; y > 1e-30L; y /= 10){
        n++;
    }
    if(n != 50) fail |= 64;
    return fail;
}

int convert(void){
    int fail = 0;
    long double a = -7.75L;
    if((int)a != -7 || (long)a != -7 || (signed char)a != -7 || (short)a != -7) fail |= 1;
    if(sizeof(long) == 8){
        /* 64-bit integers (long long is 4 bytes on SamCompiler's x86). */
        unsigned long u = 18000000000000000005ul;
        long double lu = u;
        if((unsigned long)lu != u) fail |= 2;
        long l = -9000000000000000001L;
        long double ll = l;
        if((long)ll != l) fail |= 8;
    }
    if((unsigned int)(long double)4000000000u != 4000000000u) fail |= 4;
    float f = 0.1f;
    long double lf = f;
    if(lf != f || (float)lf != f) fail |= 16;
    double d = 1e300;
    long double ld = d;
    double dd = d * 1e10;                       /* inf as a double */
    if(ld * 1e10 == dd) fail |= 32;            /* not as a long double */
    if((double)(ld * 1e10) != dd) fail |= 64;
    _Bool bt = (long double)0.5L, bf = (long double)0;
    if(bt != 1 || bf != 0) fail |= 128;
    int i = 3;
    long double sum = i + 0.25L + 'a' + 2u;
    if(sum != 102.25L) fail |= 256;
    double back = sum;
    if(back != 102.25) fail |= 512;
    return fail;
}

int globals(void){
    int fail = 0;
    if(g != 1.25L || gneg != -0.375L) fail |= 1;
    if(garr[0] + garr[1] + garr[2] != 1.5L || garr[3] != 1e-3L) fail |= 2;
    if(gzero != 0 || ghex != 0.1875L || gint != 7) fail |= 4;
    if(gbig != 1e310L || gbig / 1e10L != 1e300L) fail |= 8;
    if(gtiny * 1e10L != 1e-300L) fail |= 16;
    if(gdiv != 0.25L) fail |= 32;
    if(sizeof(long double) != sizeof g || sizeof garr != 4 * sizeof(long double)) fail |= 64;
    return fail;
}

int assign(void){
    int fail = 0;
    long double a = 1;
    double d = 2;
    int i = 3;
    a += d;
    a *= i;
    a -= 0.5L;
    a /= 2;
    if(a != 4.25L) fail |= 1;
    d += a;
    i *= a;
    if(d != 6.25 || i != 12) fail |= 2;
    long double b = a++;
    long double c = --a;
    if(b != 4.25L || c != 4.25L || a != 4.25L) fail |= 4;
    long double arr[3];
    arr[0] = 1;
    arr[1] = arr[0] + 0.5L;
    arr[2] = arr[1] * arr[1];
    if(arr[2] != 2.25L) fail |= 8;
    long double* p = &arr[1];
    *p += 1;
    p[1] = -*p;
    if(arr[1] != 2.5L || arr[2] != -2.5L) fail |= 16;
    long double t = i > 5 ? a : d;
    long double e = i < 5 ? 1.5L : 2;
    if(t != 4.25L || e != 2) fail |= 32;
    long double x, y;
    x = y = 0.75L;
    if(x != 0.75L || y != 0.75L) fail |= 64;
    return fail;
}

int main(void){
    int fail = arith() | compare() << 10 | convert() << 17;
    int more = globals() | assign() << 7;
    return (fail % 251) + (fail != 0) + (more % 251) + (more != 0);
}
