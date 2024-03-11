struct a { char name[5]; int n; short t[3]; };
int main(void){
    struct a x;
    x.name[0] = 7; x.name[4] = 1; x.n = 20; x.t[2] = 9;
    return sizeof(struct a) + x.name[0] + x.name[4] + x.n + x.t[2];
}
