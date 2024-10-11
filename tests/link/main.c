// tests/300-link-multi-file.sh: the object with main. Uses lib.c's
// variables through extern (also inside a function), its functions
// directly and by address, and libc's (printf, putchar, environ),
// which a PIE reaches through the GOT. The exit status is a bit mask
// of the failed checks.
int printf(const char* fmt, ...);
int putchar(int c);
extern char** environ;

extern int counter;
extern int gs;
extern int t0[2];
int shared = 5;

int bump(int by);
int x1(int v);

static int helper(int v){ return v + 1000; }

// lib.c's total, declared only inside the functions.
static int read_total(void){ extern int total; return total; }
int twice_total(void){ extern int total; return total * 2; }

int main(void){
    int failed = 0;
    if(bump(3) != 21) failed |= 1;
    if(counter != 21) failed |= 2;
    if(helper(1) != 1001) failed |= 4;
    int (*f)(int) = x1;
    if(f(1) != 9) failed |= 8;
    gs = 7;
    t0[1] = 0;
    if(x1(1) != 8) failed |= 16;
    if(!environ) failed |= 32;
    if(read_total() != 77 || twice_total() != 154) failed |= 64;
    int (*put)(int) = putchar;
    put('>');
    printf(" link %d %d\n", counter, gs);
    return failed;
}
