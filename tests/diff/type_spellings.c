// type spellings glibc's headers use: `long long int`,
// `unsigned long long int`, `long signed int`, `short unsigned int`,
// `int long`, in parameters, typedefs and casts; and a form feed
// between declarations (C11 6.4p3), which <sys/stat.h> has.
#include <stdio.h>
typedef long long int ll_t;
typedef unsigned long long int ull_t;

static long long int f(long long int x, unsigned long long int y, long int z, short int s,
                       unsigned short int us, signed char c, long signed int lsi, int long il){
    return x + (long long int)y + z + s + us + c + lsi + il;
}

static short unsigned int g(short unsigned int v){ return (short unsigned int)(v + 1); }

int main(void){
    long long int a = 1;
    ll_t b = 2;
    ull_t c = 3;
    long unsigned int d = 4;
    printf("%d %d %d %d\n", (int)f(a, b, c, 4, 5, 6, 7, 8), (int)d, (int)sizeof(long int), (int)sizeof(short unsigned int));
    printf("%d %d\n", g(65535), (int)sizeof(ll_t) >= 4);
    return 0;
}
