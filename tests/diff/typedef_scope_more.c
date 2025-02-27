// More typedef scopes: a typedef's own typedef names are the
// ones visible where it is declared, an array typedef of an array
// typedef, prototypes with typedef parameters, a typedef name as a
// label, a member and a declarator after unsigned, an enumerator and a
// static local hiding one, and a typedef inside a struct member list's
// type.
typedef int T;
typedef long buf[3];
typedef buf two[2];

static int captured(void){
    typedef T* P;
    typedef char T;
    P p;
    T c = 1;
    // P is still a pointer to int.
    return (int)sizeof(*p) + (int)sizeof(T) + c;
}

static long arrays(void){
    two t;
    long k = 0;
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 3; j++){
            t[i][j] = k++;
        }
    }
    return t[1][2] + (long)(sizeof(two) / sizeof(long)) + (long)(sizeof(t[0]) / sizeof(long));
}

static int proto(T);
static int proto(T x){
    return x + 1;
}

struct holder { T T; T other; };

static int names(void){
    struct holder h;
    h.T = 4;
    h.other = 5;
    struct holder* hp = &h;
    int total = hp->T + h.other;
    goto T;
T:
    total += 10;
    {
        unsigned T = 3;
        total += T;
    }
    return total;
}

static int hidden(void){
    int total = 0;
    {
        enum { T = 20 };
        total += T;
    }
    {
        static int T = 30;
        total += T;
    }
    T back = 2;
    return total + back;
}

static int sizes(void){
    typedef struct { T a; T b[4]; } rec;
    rec r;
    r.b[3] = 6;
    return (int)sizeof(rec) + r.b[3];
}

// Types that begin with qualifiers, as parameters and members.
typedef unsigned long ul;
typedef unsigned char uc;
typedef const char cchar;

struct q { ul big; uc small; };

static ul qualified(ul x, uc y, cchar* s){
    struct q v = { x, y };
    uc wrap = 300;
    char plain = 200;
    return v.big + v.small + (wrap == 44) + (plain > 0) + s[1];
}

int main(void){
    if(qualified(1000, 200, "ab") != 1000 + 200 + 1 + ((char)200 > 0) + 'b') return 7;
    if(captured() != 4 + 1 + 1) return 1;
    if(arrays() != 5 + 6 + 3) return 2;
    if(proto(4) != 5) return 3;
    if(names() != 9 + 10 + 3) return 4;
    if(hidden() != 52) return 5;
    if(sizes() != 20 + 6) return 6;
    return 0;
}
