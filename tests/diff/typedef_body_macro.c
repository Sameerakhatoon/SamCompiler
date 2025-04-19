// a typedef whose struct, union or enum body comes out of a
// macro, as in glibc's <bits/types.h> (`#define __FSID_T_TYPE struct {
// int __val[2]; }` then `__STD_TYPE __FSID_T_TYPE __fsid_t;`), a body
// with directives in it, a qualified struct typedef and several
// declarators after one body.
#define STD_TYPE typedef
#define PAIR_TYPE struct { int val[2]; }
#define WITH_EXTRA 1

STD_TYPE PAIR_TYPE pair_t;
typedef PAIR_TYPE other_pair_t, *other_pair_ptr;

typedef struct tagged {
    int a;
#if WITH_EXTRA
    int extra;
#else
    char missing;
#endif
#ifdef NOT_DEFINED
    long gone;
#endif
} tagged_t, *tagged_ptr, tagged_arr[2];

typedef const struct { int k; } const_t;

#define MAKE_ENUM(name) typedef enum { name##_A = 3, name##_B } name##_e;
MAKE_ENUM(color)

typedef union { int i; char c[8]; } bytes_t;

int main(void){
    pair_t p = { { 1, 2 } };
    other_pair_t q = { { 3, 4 } };
    other_pair_ptr qp = &q;
    tagged_t t = { 5, 6 };
    tagged_ptr tp = &t;
    tagged_arr two = { { 7, 8 }, { 9, 10 } };
    const_t c = { 11 };
    color_e e = color_B;
    bytes_t b;
    b.i = 0;
    b.c[0] = 12;
    int sum = p.val[0] + p.val[1] + qp->val[0] + q.val[1] + tp->a + tp->extra
            + two[0].a + two[1].extra + c.k + e + b.i;
    if(sizeof(tagged_t) != 2 * sizeof(int) || sizeof(two) != 4 * sizeof(int)){
        return 1;
    }
    return sum == 1 + 2 + 3 + 4 + 5 + 6 + 7 + 10 + 11 + 4 + 12 ? 0 : 2;
}
