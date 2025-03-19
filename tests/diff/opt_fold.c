// constant folding and identities under -O1 give what the
// unoptimised code computes.
int g = 7;
volatile int vg = 3;

int ident(int x, long l, unsigned u, char c, int* p){
    int r = 0;
    r += (x + 0) - (0 + x) + (x - 0) + (x * 1) + (1 * x) + (x / 1);
    r += (x << 0) + (x >> 0) + (x | 0) + (x ^ 0) + (x * 0) + (0 & x);
    r += (int)(l + 0) + (int)(u * 1) + (c + 0) + (c * 0);
    r += *(p + 0) + *(0 + p) + *(p - 0);
    r += vg * 0 + (vg + 0);
    return r;
}

int main(void){
    int s = 0;
    s += 2 + 3 * 4;                     // 14
    s += (7 / 2) + (-7 / 2) + (-7 % 3); // 3 - 3 - 1
    s += (1 << 4) | 3;                  // 19
    s += (unsigned)-1 > 0;              // 1
    s += -1 < 0u;                       // 0
    s += (char)300;                     // 44
    s += (unsigned char)-1 == 255;      // 1
    s += (short)70000;                  // 4464
    s += !5 + !0 + ~0;                  // 0
    s += 0 && g;                        // 0
    s += 1 || g;                        // 1
    s += 1 ? 2 : 3;                     // 2
    s += (2 > 1) ? (3 ^ 1) : 9;         // 2
    s += sizeof(char) + sizeof(int);    // 5
    long big = 2147483647L + 1;
    s += big > 0;                       // 1
    s += ident(g, 5L, 6u, 'a', &g);
    s += (int)((unsigned)g * 0u) + g * 1;
    return s & 0x7f;
}
