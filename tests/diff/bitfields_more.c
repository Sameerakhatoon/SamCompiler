// More bit-field uses: in a union, in a nested struct reached through
// pointers and arrays, from a struct a call returns, as call
// arguments and in conditions, designated initializers, and a struct
// passed by value.
union word {
    unsigned all;
    struct { unsigned lo : 8; unsigned mid : 16; unsigned hi : 8; } part;
};

struct cell { short id; unsigned used : 1; int score : 7; };
struct grid { int n; struct cell cells[3]; };

struct cell make(int id, int score){
    struct cell c = { .score = score, .id = (short)id, .used = 1 };
    return c;
}

int total(struct cell c){
    return c.id + c.score * (c.used ? 2 : 1);
}

int add(int a, int b){ return a + b; }

int main(void){
    union word w;
    w.all = 0;
    w.part.mid = 0xabcd;
    w.part.hi = 0x12;
    if(w.all != 0x12abcd00u || w.part.lo != 0) return 1;
    struct grid g = { 3, { { 1, 1, -5 }, [2] = { .id = 9, .score = 60 } } };
    struct grid* p = &g;
    p->cells[1].score = -64;
    p->cells[1].used = p->cells[0].used;
    if(p->cells[1].score != -64 || !p->cells[1].used || g.cells[2].used) return 2;
    if(make(4, -3).score != -3 || make(4, 70).score != -58) return 3;
    if(total(make(2, 10)) != 22 || total(g.cells[0]) != -9) return 4;
    if(add(g.cells[2].score, p->cells[0].score) != 55) return 5;
    int n = 0;
    for(int i = 0; i < 3; i++){
        if(g.cells[i].used){
            n += g.cells[i].score;
        }
    }
    g.cells[2].score += 10;
    g.cells[2].score *= 2;
    return n + 70 + g.cells[2].score;
}
