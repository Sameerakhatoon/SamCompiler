// EXPECT: 2 5 7
// #else, also inside a function and around a nested #if.
int printf(const char* fmt, ...);
#ifdef NOPE
int g = 1;
#else
int g = 2;
#endif
#define A 1
int f(int x){
#if A
    return x + 1;
#else
    (void)x;
#if A
    return 9;
#endif
    return 0;
#endif
}
int seven(){
#ifndef A
    return 1;
#else
    return 7;
#endif
}
int main(){ printf("%d %d %d\n", g, f(4), seven()); return 0; }
