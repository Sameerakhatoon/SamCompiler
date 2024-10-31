/* A21: float and double arithmetic and the usual arithmetic conversions. */
int main(void){
    int fail = 0;
    float h = 7.0f;
    double d = 2.5;
    int i = 3;
    if((int)((7.0f / 2) * 2) != 7) fail |= 1;
    if((h / 2) * 2 != 7) fail |= 2;
    if(d * i != 7.5) fail |= 4;
    if(i / 2 * d != 2.5) fail |= 8;          /* int division first */
    if(i / 2.0 != 1.5) fail |= 16;
    if(1 - d != -1.5) fail |= 32;
    if(-d + 0.5 != -2.0) fail |= 64;
    if(sizeof(h * 2) != 4 || sizeof(h * 2.0) != 8 || sizeof(i + 1.0f) != 4) fail |= 128;
    unsigned u = 3000000000u;
    double ud = u / 2.0;
    if(ud != 1500000000.0) fail |= 256;
    float third = 1.0f / 3;
    double dthird = 1.0 / 3;
    if(third == dthird) fail |= 512;          /* float rounding is kept */
    if((float)dthird != third) fail |= 1024;
    char c = 'A';
    if(c * 0.5 != 32.5) fail |= 2048;
    return fail % 255;
}
