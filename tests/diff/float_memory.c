/* A21: float and double globals (initialized), arrays, struct members
   and pointers. */
struct point { char tag; double x; float y; int n; };

double gd = 1.5;
float gf = -2.25f;
double gz;
double garr[4] = { 1.0, 2.5, -3.0 };
float farr[3] = { 0.5f, 1 };
struct point gp = { 'p', 3.25, 0.5f, 7 };
double folded = 2.0 * 3.0 + 1 / 4.0;
float narrowed = 1.0 / 3;
int truncated = 7.9;
static double hidden = -0.0;

double sum(double* v, int n){
    double s = 0;
    for(int i = 0; i < n; i++) s += v[i];
    return s;
}

int main(void){
    int fail = 0;
    if(gd + gf != -0.75 || gz != 0) fail |= 1;
    if(garr[1] * garr[2] != -7.5 || garr[3] != 0) fail |= 2;
    if(farr[0] + farr[1] + farr[2] != 1.5f) fail |= 4;
    if(gp.x + gp.y + gp.n != 10.75 || gp.tag != 'p') fail |= 8;
    if(folded != 6.25 || narrowed != 1.0f / 3 || truncated != 7) fail |= 16;
    if(hidden != 0 || 1 / hidden > 0) fail |= 32;
    struct point lp;
    lp.x = 2.0;
    lp.y = lp.x / 4;
    struct point* pp = &lp;
    pp->x += pp->y;
    if(lp.x != 2.5 || lp.y != 0.5f) fail |= 64;
    double* p = &garr[0];
    p[2] += 1;
    *p = *p * 3;
    if(garr[2] != -2.0 || garr[0] != 3.0) fail |= 128;
    double local[3] = { 1, 2, 3.5 };
    if(sum(local, 3) != 6.5 || sum(garr, 4) != 3.5) fail |= 256;
    struct point copy = gp;
    if(copy.x != 3.25) fail |= 512;
    float m[2][2] = { { 1, 2 }, { 3, 4 } };
    if(m[1][0] * m[0][1] != 6) fail |= 1024;
    return fail % 255;
}
