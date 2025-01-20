/* <stdbool.h>'s bool is _Bool: a value stored in one is 0 or 1. */
#include <stdbool.h>
struct flags { int bits; };
static bool has(struct flags* f, int bit){ return f->bits & bit; }
static bool same(bool a, bool b){ return a == b; }
int main(void){
    struct flags f = { 0x900 };
    bool b = 0x200;
    bool c = f.bits & 0x800;
    if(!has(&f, 0x800) || has(&f, 0x400)) return 1;
    if(b != 1 || c != true || sizeof(bool) != 1) return 2;
    if(!same(0x100, 1)) return 3;
    b = 0;
    b += 2;
    return b == 1 ? 0 : 4;
}
