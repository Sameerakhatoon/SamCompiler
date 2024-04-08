int a(void){ static int n; return ++n; }
int b(void){ static int n = 100; n += 10; return n; }
int c(int reset){
    static int calls = 0, total = 5;
    if (reset) { int calls = 50; return calls; }
    calls++;
    total += calls;
    return total;
}
int main(void){
    a(); a();
    b();
    c(0); c(0);
    return (a() + b() + c(0) + c(1)) / 2;
}
