// Bit-fields: the layout gcc gives them (sizeof and the bytes), signed
// and unsigned reads, stores that keep the neighbours, compound
// assignment, ++ and --, unnamed and zero-width fields, a field
// sharing its unit with a char before it, fields through pointers and
// in arrays, and global and local initializers.
struct flags {
    unsigned ready : 1;
    unsigned mode  : 3;
    int      delta : 5;
    unsigned       : 2;
    unsigned big   : 20;
};

struct mixed {
    char tag;
    int  low : 4;
    int  high : 12;
    char after;
};

struct split {
    unsigned a : 30;
    unsigned b : 4;
    unsigned   : 0;
    unsigned c : 3;
    short s;
};

struct small {
    unsigned char x : 3;
    unsigned char y : 5;
    signed char   z : 4;
};

struct flags g = { 1, 5, -3, 1000 };
struct mixed gm = { 'q', -2, 300, 'z' };

int bytes_sum(void* p, int n){
    unsigned char* b = p;
    int s = 0;
    for(int i = 0; i < n; i++){
        s = s * 7 + b[i];
    }
    return s & 0xffff;
}

int bump(struct flags* f){
    f->mode = f->mode + 2;
    f->delta -= 4;
    return f->mode * 100 + f->delta;
}

int main(void){
    if(sizeof(struct flags) != 4 || sizeof(struct mixed) != 4 || sizeof(struct split) != 12 || sizeof(struct small) != 2) return 1;
    if(g.ready != 1 || g.mode != 5 || g.delta != -3 || g.big != 1000) return 2;
    if(gm.tag != 'q' || gm.low != -2 || gm.high != 300 || gm.after != 'z') return 3;
    struct flags f = { 0, 7, 15, 0xfffff };
    if(f.mode != 7 || f.delta != 15 || f.big != 0xfffff || f.ready) return 4;
    f.delta = -16;
    f.mode = 9;
    if(f.delta != -16 || f.mode != 1 || f.big != 0xfffff) return 5;
    if(f.mode - 2 >= 0) return 6;
    f.ready++;
    f.ready++;
    if(f.ready != 0) return 7;
    int old = f.mode--;
    if(old != 1 || f.mode != 0) return 8;
    if(++f.mode != 1) return 9;
    f.big += 2;
    if(f.big != 1) return 10;
    int r = bump(&f);
    if(r != 300 + 12) return 11;
    struct split s;
    s.a = 0x3fffffff;
    s.b = 9;
    s.c = 5;
    s.s = -1;
    if(s.a != 0x3fffffff || s.b != 9 || s.c != 5 || s.s != -1) return 12;
    struct small t[2];
    t[1].x = 6;
    t[1].y = 31;
    t[1].z = -7;
    t[0] = t[1];
    int stored = (t[0].x = 9);
    t[0].y = stored + 1;
    if(t[0].x != 1 || t[0].y != 2 || t[0].z != -7 || t[1].y != 31) return 13;
    struct mixed m = { 1, 7, -1, 2 };
    m.low = m.low + m.high;
    if(m.low != 6 || m.high != -1 || m.tag != 1 || m.after != 2) return 14;
    return (bytes_sum(&g, 4) + bytes_sum(&gm, 4) + bytes_sum(&f, 4) + bytes_sum(&m, 4)) % 251;
}
