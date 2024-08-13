typedef int (*unary)(int);
int twice(int x){ return x * 2; }
int plus3(int x){ return x + 3; }
struct entry { unary f; int arg; };

int main(void){
    struct entry t[2] = { { twice, 5 }, { plus3, 4 } };
    struct entry e;
    e.f = plus3;
    e.arg = 1;
    struct entry* q = &e;
    int i = 1;
    return t[0].f(t[0].arg) + t[i].f(t[i].arg) + q->f(q->arg)
        + (*t[0].f)(1) + (*q->f)(2);
}
