// EXPECTED EXIT: 111
// Chains of one left-to-right operator group to the left:
// (a - b) - c, not a - (b - c).
// Two blocks instead of #else.
#if 10 - 3 - 2 == 5
#define PP_CHAIN 10
#endif
#ifndef PP_CHAIN
#define PP_CHAIN 0
#endif

int main()
{
    int a = 100;
    int b = 30;
    int c = 20;
    int sub = a - b - c;
    int d = 64;
    int div = d / 4 / 2;
    int mod = a % b % 7;
    int one = 1;
    int shl = one << 2 << 3;
    int big = 256;
    int shr = big >> 2 >> 3;
    // 50 + 8 + 3 + 32 + 8 + 10
    return sub + div + mod + shl + shr + PP_CHAIN;
}
