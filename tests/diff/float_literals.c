/* A21: floating constants: a fraction, an exponent, the f suffix, a
   leading or trailing point, hexadecimal floats. */
#define SCALE 1e3
#define HALF .5f
int main(void){
    int fail = 0;
    if(1.5 != 3 / 2.0) fail |= 1;
    if(2e3 != 2000 || 2E-2 != 0.02 || 5e+1 != 50) fail |= 2;
    if(HALF + 1.f != 1.5f || sizeof(HALF) != 4 || sizeof(1.5) != 8) fail |= 4;
    if(10. != 10 || .25 * 4 != 1) fail |= 8;
    if(0x1p4 != 16 || 0x1.8p1 != 3 || 0X.8P0 != 0.5 || 0x10p-4f != 1) fail |= 16;
    if(SCALE * 2 != 2000) fail |= 32;
    if(1e-3f != (float)0.001) fail |= 64;
    if(123456789.0f != 123456792) fail |= 128;   /* float rounding of the constant */
    return fail % 255;
}
