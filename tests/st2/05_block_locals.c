// EXPECT: 12 7 9 5
// locals of a block statement (a case body, a bare { }) live in
// the frame; they all had offset 0, over the saved frame pointer.
int printf(const char* fmt, ...);
int f(int x){
    int a = 1;
    switch(x){
        case 1: { long long v = 5; int w = 6; return v + w + a; }
        case 2: { int q = 7; return q; }
    }
    { int z = 9; return z; }
}
int g(int x){
    int a = 1;
    for(;;){ { long v = 3; return v + a + x; } }
}
int main(){ printf("%d %d %d %d\n", f(1), f(2), f(3), g(1)); return 0; }
