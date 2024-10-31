/* A21: conversions between float, double and the integer types. */
int main(void){
    int fail = 0;
    double d = -3.99;
    if((int)d != -3) fail |= 1;            /* truncation toward zero */
    if((int)3.99 != 3) fail |= 2;
    if((char)65.7 != 'A') fail |= 4;
    if((short)-1234.9 != -1234) fail |= 8;
    if((unsigned char)200.5 != 200) fail |= 16;
    unsigned u = (unsigned)3000000000.0;
    if(u != 3000000000u) fail |= 32;
    double back = u;
    if(back != 3000000000.0) fail |= 64;
    float big = 16777217;                  /* rounds to 2^24 */
    if(big != 16777216.0f) fail |= 128;
    float f = (float)0.1;
    double fd = f;
    if(fd == 0.1) fail |= 256;
    if(fd != (double)0.1f) fail |= 512;
    _Bool b = 0.5;
    _Bool z = 0.0;
    if(b != 1 || z != 0) fail |= 1024;
    long l = 2.5e5;
    if(l != 250000) fail |= 2048;
    int i = 7;
    i = i * 1.5;                           /* 10.5 -> 10 */
    if(i != 10) fail |= 4096;
    char c = 1;
    c += 0.75;                             /* 1.75 -> 1 each time */
    c += 0.75;
    if(c != 1) fail |= 8192;
    return fail % 255;
}
