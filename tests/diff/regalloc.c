// register allocation at -O1. Locals and parameters that live
// in callee-saved registers next to ones that must stay in memory.
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int sum_to(int n){
    int s = 0;
    for(int i = 0; i < n; i++){
        s += i;
    }
    return s;
}

// More variables than any target has registers.
static long many(long a, long b, long c, long d, long e, long f, long g, long h, long i, long j){
    long v1 = a, v2 = b, v3 = c, v4 = d, v5 = e, v6 = f, v7 = g, v8 = h, v9 = i, v10 = j;
    long v11 = a + j, v12 = b + i, v13 = c + h;
    for(int k = 0; k < 3; k++){
        v1 += v2; v2 += v3; v3 += v4; v4 += v5; v5 += v6; v6 += v7; v7 += v8; v8 += v9; v9 += v10;
        v10 += v11; v11 += v12; v12 += v13; v13 += v1;
    }
    return v1 ^ v2 ^ v3 ^ v4 ^ v5 ^ v6 ^ v7 ^ v8 ^ v9 ^ v10 ^ v11 ^ v12 ^ v13;
}

// Values kept across calls that use the same registers themselves.
static int depth(int n){
    int a = n * 3;
    int b = n + 7;
    int c = 0;
    for(int i = 0; i < 4; i++){
        c += a - b + i;
    }
    if(n > 0){
        c += depth(n - 1);
    }
    return a + b + c;
}

static void bump(int* p){
    (*p)++;
}

// x has its address taken, y does not; both shadowed in a block.
static int addressed(int n){
    int x = 1;
    int y = 2;
    for(int i = 0; i < n; i++){
        bump(&x);
        y += x;
        {
            int x = i * 10;
            int y = x + 1;
            x += y;
            if(x > 1000){
                return -1;
            }
        }
    }
    return x * 100 + y;
}

// Narrow and unsigned types wrap as they would in memory.
static int narrow(void){
    signed char c = 120;
    unsigned char u = 250;
    short s = 32760;
    unsigned short us = 65530;
    unsigned int ui = 4294967290u;
    _Bool b = 0;
    int r = 0;
    for(int i = 0; i < 10; i++){
        c++;
        u += 3;
        s += 2;
        us++;
        ui += 1;
        b = i & 1;
        r += c + u + s + us + (int)(ui & 0xff) + b;
    }
    return r;
}

// Pre and post increments as values, pointers stepping by elements,
// and every compound operator.
static long operators(void){
    int a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int* p = a;
    int* q = a + 7;
    long t = 0;
    int i = 5;
    t += i++;
    t += ++i;
    t += i--;
    t += --i;
    t += *p++;
    t += *++p;
    t += *q--;
    t += q - p;
    int x = 100;
    x += 7; x -= 3; x *= 5; x /= 3; x %= 11; x <<= 4; x >>= 1; x &= 0x3f; x |= 0x100; x ^= 0x55;
    t += x;
    unsigned int y = 7;
    y -= 10;
    y >>= 3;
    t += y;
    int f = 10;
    f += 2.75;
    f *= 1.5;
    t += f;
    long double ld = 2.5L;
    int g = 3;
    g += ld;
    t += g;
    return t;
}

// A variadic function: the va_list is a local that must stay in memory.
static int vsum(int count, ...){
    va_list ap;
    va_start(ap, count);
    int total = 0;
    for(int i = 0; i < count; i++){
        int v = va_arg(ap, int);
        total += v;
    }
    va_end(ap);
    return total;
}

static int twice(int v){
    return v * 2;
}

// Function pointers, switch and goto with register variables.
static int control(int n){
    int (*fn)(int) = twice;
    int acc = 0;
    int k = 0;
again:
    switch(k % 3){
        case 0: acc += fn(k); break;
        case 1: acc -= k; break;
        default: acc ^= k; break;
    }
    k++;
    if(k < n){
        goto again;
    }
    return acc;
}

// Many parameters: the ones past the argument registers come on the
// stack.
static long params(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k){
    long s = 0;
    for(int z = 0; z < 2; z++){
        s += a * 1 + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10 + k * 11;
    }
    return s;
}

// Strings walked with a pointer in a register.
static int count_char(const char* s, char c){
    int n = 0;
    while(*s){
        if(*s == c){
            n++;
        }
        s++;
    }
    return n;
}

static volatile int vol_sink;

static int with_volatile(int n){
    volatile int v = 0;
    int w = 0;
    for(int i = 0; i < n; i++){
        v += i;
        w += v;
    }
    vol_sink = v;
    return w;
}

// Every result goes into the exit status.
static unsigned long mix(unsigned long h, long v){
    return (h ^ (unsigned long)v) * 1000003ul + 7;
}

int main(void){
    unsigned long h = 17;
    h = mix(h, sum_to(100));
    h = mix(h, many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    h = mix(h, depth(6));
    h = mix(h, addressed(5));
    h = mix(h, narrow());
    h = mix(h, operators());
    h = mix(h, vsum(5, 1, 2, 3, 4, 5));
    h = mix(h, control(10));
    h = mix(h, params(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
    h = mix(h, count_char("register allocation", 'r') + (int)strlen("abc"));
    h = mix(h, with_volatile(6));
    return (int)(h % 251);
}
