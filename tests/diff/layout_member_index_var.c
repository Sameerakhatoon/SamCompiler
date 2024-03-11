struct a { int k; int v[4]; char s[6]; };
int main(void){
    struct a x; struct a* p = &x;
    int i;
    for (i = 0; i < 4; i++) x.v[i] = i * 3;
    for (i = 0; i < 6; i++) p->s[i] = i + 1;
    x.k = 2;
    return x.v[3] + p->v[1] + x.s[5] + p->s[0] + x.k;
}
