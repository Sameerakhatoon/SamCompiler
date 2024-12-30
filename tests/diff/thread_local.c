// _Thread_local and __thread objects in one thread: globals
// with and without initializers (.tdata, .tbss), static ones, static
// locals, arrays and structs, their addresses, and an _Atomic one.
static unsigned h;

static void mix(long v){
    h = h * 31 + (unsigned)v;
}

struct pair { int a; long b; };

_Thread_local int counter = 7;
_Thread_local long zeroed;
static __thread short small = -3;
static _Thread_local int table[4] = { 1, 2, 3, 4 };
_Thread_local struct pair pr = { 5, 6 };
static _Thread_local struct pair blank;
_Thread_local _Atomic int tally;
static _Thread_local char* name = "tls";

static int next(void){
    static _Thread_local int calls;
    static _Thread_local int base = 100;
    calls++;
    return base + calls;
}

int main(void){
    mix(counter);
    counter += 5;
    mix(counter);
    mix(zeroed);
    zeroed = 1L << 20;
    mix(zeroed >> 10);
    mix(small);
    small *= 1000;
    mix(small);
    for(int i = 0; i < 4; i++){
        table[i] *= i + 1;
        mix(table[i]);
    }
    mix(pr.a + pr.b);
    pr.b = 40;
    blank = pr;
    mix(blank.a + blank.b);
    int* p = &counter;
    *p = 99;
    mix(counter);
    tally++;
    tally += 2;
    mix(tally);
    mix(next());
    mix(next());
    // Through a local: i386 miscompiles `(*pp)[1]`, which is what
    // `name[1]` becomes there.
    char* n = name;
    mix(n[1]);
    return h % 127;
}
