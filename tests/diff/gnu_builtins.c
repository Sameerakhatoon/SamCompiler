// _Static_assert at file scope and in a block, asm labels on
// function declarations (glibc's __REDIRECT: `int f(...) __asm__("g")`
// makes f's symbol g), __builtin_expect, __builtin_constant_p of a value
// that is not constant, __builtin_offsetof and the byte swaps.
#include <stdio.h>

struct pair { char tag; long value; int rest[3]; };

_Static_assert(sizeof(int) == 4, "int is 4 bytes");
_Static_assert(sizeof(struct pair) > 2 * sizeof(int), "a pair is bigger than two ints");

extern int absolute(int) __asm__("abs");
extern unsigned long length_of(const char* s) __asm__ ("str" "len");

static int twice(int x) __asm__("twice_renamed");
static int twice(int x){ return 2 * x; }

int main(int argc, char** argv){
    _Static_assert(sizeof(struct pair) >= sizeof(long), "a pair holds a long");
    int n = argc + 4;
    if(__builtin_expect(n > 100, 0)){
        return 1;
    }
    int (*f)(int) = absolute;
    printf("%d %d %lu %d\n", absolute(-n), f(-3), length_of("hello"), twice(21));
    printf("%d %lu %lu\n", __builtin_constant_p(argc) == 0, __builtin_offsetof(struct pair, value),
           __builtin_offsetof(struct pair, rest[1]));
    unsigned short h = 0x1234;
    unsigned int w = 0x11223344u;
    printf("%x %x %x\n", __builtin_bswap16(h), __builtin_bswap32(w), __builtin_bswap32(0xa1b2c3d4u) & 0xffu);
#ifdef __LP64__
    unsigned long long q = 0x0102030405060708ull;
    printf("%llx\n", (unsigned long long)__builtin_bswap64(q));
#endif
    return argv[0] ? 0 : 2;
}
