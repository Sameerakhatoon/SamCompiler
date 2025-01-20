/* C11 anonymous struct and union members: access, layout,
   designated and positional initializers, bit-fields. */
struct tok {
    int type;
    union {
        char cval;
        const char* sval;
        unsigned long long llnum;
    };
    int after;
};

struct shape {
    int kind;
    union {
        struct { int w; int h; };
        struct { int r; long area; };
    };
    struct {
        char tag;
        int bits : 4;
        int more : 4;
    };
};

union value {
    struct { short lo; short hi; };
    int whole;
};

static struct tok global_tok = { .type = 3, .llnum = 99, .after = 5 };
static struct shape global_shape = { 1, { { 2, 3 } } };

static int area(struct shape* s){
    return s->kind == 1 ? s->w * s->h : s->r * 3;
}

int main(void){
    struct tok t = { .type = 1, .sval = "abc", .after = 7 };
    struct tok z = { 0 };
    struct shape s;
    struct shape* p = &s;
    union value v;
    if(sizeof(struct tok) < sizeof(int) * 2 + sizeof(long long)) return 1;
    if(t.sval[1] != 'b' || t.after != 7 || t.type != 1) return 2;
    t.llnum = 0x4142;
    if(t.cval != 0x42) return 3;
    if(global_tok.llnum != 99 || global_tok.after != 5 || global_tok.type != 3) return 4;
    if(z.llnum != 0 || z.after != 0) return 5;
    s.kind = 1;
    p->w = 6;
    p->h = 7;
    if(area(p) != 42) return 6;
    s.kind = 2;
    if(area(&s) != 18) return 7;
    p->area = 1000;
    if(s.area != 1000 || s.w != 6) return 8;
    s.tag = 'x';
    s.bits = 5;
    s.more = -2;
    if(s.tag != 'x' || s.bits != 5 || s.more != -2) return 9;
    if(area(&global_shape) != 6) return 10;
    v.whole = 0x00020001;
    if(v.lo != 1 || v.hi != 2) return 11;
    v.hi = 3;
    if(v.whole != 0x00030001) return 12;
    if(sizeof(union value) != 4) return 13;
    return 0;
}
