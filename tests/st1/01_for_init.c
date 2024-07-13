// EXPECT: 6 30 2
// `for(int i = ...)` declares i in a scope around the loop, and
// the locals of a for body are on the frame.
int printf(const char* fmt, ...);
int main(){
    int s = 0;
    for(int i = 0; i < 4; i = i + 1){ s = s + i; }
    int t = 0;
    for(int i = 0; i < 3; i = i + 1){ char buf[128]; int r = 10; buf[0] = 1; t = t + r * buf[0]; }
    int n = 0;
    for(int j = 0; j < 2; j = j + 1){ n = n + 1; }
    printf("%d %d %d\n", s, t, n);
    return 0;
}
