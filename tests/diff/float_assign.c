/* A21: assignment, compound assignment and ++/-- on float and double. */
int main(void){
    int fail = 0;
    double x = 1;
    x++;
    ++x;
    x -= 0.5;
    x *= 4;
    x /= 5;
    if(x != 2.0) fail |= 1;
    float y = 1.5f;
    float old = y--;
    if(old != 1.5f || y != 0.5f) fail |= 2;
    float pre = --y;
    if(pre != -0.5f || y != -0.5f) fail |= 4;
    double a, b;
    a = b = 2.25;
    if(a + b != 4.5) fail |= 8;
    int n = 10;
    n -= 2.5;                              /* 7.5 -> 7 */
    if(n != 7) fail |= 16;
    double big = 1e16;
    double big_old = big++;                /* 1e16 + 1 rounds back */
    if(big != big_old) fail |= 32;
    double s = 0.1;
    double s_old = s++;
    if(s_old != 0.1) fail |= 64;           /* not (0.1 + 1) - 1 */
    double v = (x = 3.5) + 1;
    if(v != 4.5 || x != 3.5) fail |= 128;
    return fail;
}
