// A struct read whole through a pointer, through a cast pointer, and a
// member of `*p`: x86 loaded the struct's first dword as its address.
struct s12 { int a, b, c; };
struct s12 g = { 2, 3, 4 };

void* get(void){ return &g; }

int main(void){
    void* p = &g;
    struct s12* q = &g;
    struct s12 a = *q;
    struct s12 b;
    b = *(struct s12*)p;
    struct s12 c = *(struct s12*)get();
    int m = (*q).c + (*(struct s12*)p).b;
    return a.a + 2 * b.c + 3 * c.b + 5 * m;
}
