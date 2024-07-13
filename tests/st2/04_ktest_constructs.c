// EXPECT: hi 32 5 3 9 4 64 3 c 2
// what ktest and rvtest use: a cast to a function pointer type,
// `(void)x;`, struct members declared as a list, `sizeof x`, a
// trailing comma in a brace initializer, basic inline asm, #elif.
int printf(const char* fmt, ...);
struct cs { unsigned long index, apic, busy; char c, d; int e; };
struct cs g[2];
void hi(void){ printf("hi "); }
#if 0
#define PICK 1
#elif defined(__LP64__)
#define PICK 2
#else
#define PICK 3
#endif
int main(){
    struct cs a; a.busy = 5; a.d = 3; a.e = 9; g[1].apic = 4;
    unsigned long addr = (unsigned long)hi;
    ((void (*)(void))addr)();
    (void)a;
    __asm__ volatile("nop");
    __asm__ volatile("nop" ::: "memory");
    char b[64];
    const char* x[] = { "a", "b", "c", };
    printf("%d %d %d %d %d ", (int)sizeof(struct cs), (int)a.busy, a.d, a.e, (int)g[1].apic);
    printf("%d %d %s %d\n", (int)sizeof b, (int)(sizeof x / sizeof x[0]), x[2], PICK);
    return 0;
}
