// const per pointer level: every allowed form compiles and
// runs as with gcc.
struct s { int* p; const char* name; int a[2]; };

static int sum(char const* const* names, int count){
    int total = 0;
    for(int i = 0; i < count; i++){
        total += names[i][0];
    }
    return total;
}

int main(void){
    int v = 3, u = 5;
    const int* cp = &v;
    int* const pc = &u;
    const int* const cpc = &v;
    int const* also = &u;
    int* volatile vp = &v;
    int* restrict rp = &u;
    cp = &u;
    *pc = 7;
    also = cp;
    vp = rp;
    const int table[3] = { 1, 2, 3 };
    int* const pointers[2] = { &v, &u };
    *pointers[0] = 4;
    char* words[2];
    words[0] = "a";
    words[1] = "b";
    struct s x;
    x.p = &v;
    x.name = "q";
    x.a[0] = 1;
    const struct s* sp = &x;
    struct s* const ps = &x;
    *sp->p = 9;
    ps->a[1] = 2;
    int* drop = (int*)cp;
    const void* cv = cp;
    void* plain = (void*)cv;
    return (*cp + *pc + *cpc + *also + *vp + table[2] + *drop + (plain != 0)
        + sum((char const* const*)words, 2) + sp->name[0] + ps->a[1] + v) % 127;
}
