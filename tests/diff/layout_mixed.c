struct m { char c; int i; char d; short s; long l; char* p; };
int main(void){
    struct m x;
    x.c = 1; x.i = 2; x.d = 3; x.s = 4; x.l = 5; x.p = 0;
    return sizeof(struct m) * 4 + x.c + x.i + x.d + x.s + x.l + (x.p == 0);
}
