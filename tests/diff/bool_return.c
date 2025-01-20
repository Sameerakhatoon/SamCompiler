/* A value converted to _Bool is 0 or 1 (C11 6.3.1.2), in a return, an
   argument and a call's result too, not its low byte. */
static _Bool has_bit(int flags){ return flags & 0x800; }
static _Bool nonzero(long v){ return v; }
static int count(_Bool b){ return b; }
struct s { _Bool on; };
static _Bool via_pointer(struct s* p, int x){ p->on = x & 0x100; return p->on; }

int main(void){
    struct s s;
    if(!has_bit(0x800)) return 1;
    if(has_bit(0x400)) return 2;
    if(nonzero(0x10000L) != 1) return 3;
    if(count(0x200) != 1) return 4;
    if(!via_pointer(&s, 0x100) || s.on != 1) return 5;
    _Bool b = has_bit(0x801);
    return b == 1 ? 0 : 6;
}
