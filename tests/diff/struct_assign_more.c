struct in { int a; long b; char c; };
struct out { int k; struct in i; struct in arr[2]; };

int main(void){
    struct in x = { 1, 2, 3 };
    struct out o;
    struct out* p = &o;
    o.i = x;
    p->arr[1] = x;
    p->arr[0] = o.i;
    p->arr[0].b = 9;
    struct in y;
    struct in* yp = &y; *yp = x;
    return o.i.a + o.i.b + o.i.c + p->arr[1].b + p->arr[0].b + p->arr[0].c + y.c;
}
