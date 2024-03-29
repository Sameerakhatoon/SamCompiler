#ifdef __LP64__
#define BIG (1L << 33)
#else
#define BIG 77
#endif
int f(long v){
    switch (v) {
        case 0: return 1;
        case BIG: return 2;
        case -1: return 3;
        default: return 4;
    }
}
int main(void){ return f(BIG) * 10 + f(0) + f(-1) * 100 + f(5) * 0; }
