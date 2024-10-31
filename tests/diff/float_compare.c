/* A21: comparisons, truth values, NaN and -0.0, && || ?: and loops. */
int main(void){
    int r = 0;
    double one = 1.0, two = 2.0, zero = 0.0;
    double nan = zero / zero;
    double negzero = -zero;
    r += (one < two) + (one <= one) * 2 + (two > one) * 4 + (two >= two) * 8;
    r += (one == one) * 16 + (one != two) * 32;
    /* Every ordered comparison with a NaN is false, != is true. */
    r += (nan < one) + (nan > one) + (nan <= one) + (nan >= one) + (nan == nan);
    if(nan != nan) r += 64;
    if(negzero) r += 1000;                 /* -0.0 is false */
    if(!negzero) r += 1;
    if(nan) r += 2;                        /* a NaN is true */
    if(one && 0.5) r += 4;
    if(zero || negzero) r += 1000;
    float f = 0.25f;
    double sel = f > 0 ? f : 2;
    if(sel != 0.25) r += 1000;
    int n = 0;
    for(double t = 0; t < 1.0; t += 0.125) n++;
    while(f < 3) f = f * 2;
    do { n++; } while(zero);
    return (r + n + (int)f) % 128;
}
