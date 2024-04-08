struct big { int a; long b; char c[5]; int d; };
struct big make(int k){ struct big r = { k, k * 2, "abc", k * 3 }; return r; }
int main(void){
    struct big x = { 1, 2, "xy", 4 };
    struct big y = x;
    struct big z = make(5);
    return y.a + y.b + y.c[1] + y.d + z.a + z.b + z.c[2] + z.d;
}
