// typedef names have C's scopes: a block scope typedef, a
// typedef hidden by a variable or by another typedef in an inner block
// and visible again after it, a parameter hiding one, the same name as a
// typedef, a struct tag and a member, a typedef in a for loop's body and
// a function body, and sizeof, casts and compound literals through
// typedef names.
typedef int T;
typedef struct T { T T; char c; } S;
typedef T* TP;

static int size_of_t(void){
    return sizeof(T);
}

static int param_hides(int T){
    // T is the parameter here, an int.
    return T * 2;
}

static int inner_typedef(void){
    int total = 0;
    {
        typedef char T;
        T small = 300;
        total += sizeof(T) + small;
        {
            T T = 5;
            total += T;
        }
        total += sizeof(T);
    }
    // The file scope T again.
    total += sizeof(T);
    return total;
}

static int hidden_by_variable(void){
    int total = 0;
    {
        long T = 7;
        total += T * 3;
        total += sizeof(T);
    }
    T x = 40;
    total += x + sizeof(T);
    return total;
}

static int struct_and_member(void){
    S s;
    s.T = 9;
    s.c = 2;
    struct T t = { 3, 4 };
    TP p = &s.T;
    *p += 1;
    return s.T + s.c + t.T + t.c;
}

static int in_loops(void){
    int total = 0;
    for(T i = 0; i < 4; i++){
        typedef short W;
        W w = (W)(i * 100);
        total += w + sizeof(W);
    }
    for(int i = 0; i < 2; i++){
        typedef long W;
        total += sizeof(W) + (W)i;
    }
    return total;
}

typedef struct { int a; int b; } pair;

static int casts_and_literals(void){
    typedef unsigned char byte;
    int total = (byte)511;
    total += (int)sizeof(byte) + (int)sizeof(pair);
    pair p = (pair){ 5, 6 };
    total += p.a * p.b;
    total += ((pair){ 1, 2 }).b;
    byte* b = (byte*)&total;
    total += b[0] != 0;
    return total;
}

typedef int (*op)(int);

static int twice(int x){
    return x * 2;
}

static int through_pointers(void){
    op f = twice;
    typedef op alias;
    alias table[2] = { twice, twice };
    return f(3) + table[1](4) + (int)(sizeof(table) / sizeof(alias));
}

int main(void){
    if(size_of_t() != 4) return 1;
    if(param_hides(21) != 42) return 2;
    if(inner_typedef() != 1 + 44 + 5 + 1 + 4) return 3;
    if(hidden_by_variable() != 21 + (int)sizeof(long) + 40 + 4) return 4;
    if(struct_and_member() != 10 + 2 + 3 + 4) return 5;
    if(in_loops() != (0 + 100 + 200 + 300 + 4 * 2) + (2 * (int)sizeof(long) + 1)) return 6;
    if(casts_and_literals() != 255 + 1 + 8 + 30 + 2 + 1) return 7;
    if(through_pointers() != 6 + 8 + 2) return 8;
    T T = 3;
    return T - 3;
}
