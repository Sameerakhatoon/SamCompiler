_Bool to_bool(int x){ return x; }
int main(void){
    _Bool a = 2;
    _Bool b = 256;
    _Bool c;
    c = 0;
    c = 5;
    int x = 0;
    _Bool d = (_Bool)x;
    _Bool e = 1;
    e &= 4;
    struct s { _Bool flag; int n; } v;
    v.flag = 9;
    return a + b * 2 + c * 4 + d * 8 + e * 16 + v.flag * 32 + to_bool(3) * 64 + sizeof(_Bool);
}
