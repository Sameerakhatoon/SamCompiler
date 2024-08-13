struct small { int f; int arg; };
struct big { long f; long arg; };

int main(void){
    struct small t[2];
    t[1].f = 3;
    t[1].arg = 4;
    struct small copy;
    copy = t[1];
    struct small other;
    struct small* p = &other;
    *p = t[1];
    t[0] = t[1];
    int i = 1;
    struct small v = t[i];

    struct big b[2];
    b[1].f = 5;
    b[1].arg = 6;
    struct big bc;
    struct big* bp = &bc;
    *bp = b[1];

    return copy.f + copy.arg + p->f + p->arg + t[0].f + t[0].arg
        + v.f + v.arg + (int)(bp->f + bp->arg);
}
