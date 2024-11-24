// On x86, `&q->y` gave the address of q itself plus y's offset, and
// `(*q).y = v` loaded through the struct's first member. Both are
// checked through one and two levels of pointers.
struct pt { int x; int y; };
struct box { struct pt* p; int k; };
int main(void){
    struct pt a; a.x = 1; a.y = 2;
    struct pt* q = &a;
    int* r = &q->y;
    *r = 9;
    (*q).x = 7;
    struct box b; b.p = &a; b.k = 3;
    struct box* bp = &b;
    int* s = &bp->p->x;
    *s = *s + 10;
    (*bp->p).y = (*bp).k + 30;
    int* t = &(*q).x;
    *t = *t + 1;
    return a.x + a.y * 2 + b.k;
}
