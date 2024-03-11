struct inner { char c; long l; };
struct outer { char a; struct inner in; char b; };
int main(void){
    struct outer o;
    o.a = 1; o.in.c = 2; o.in.l = 30; o.b = 4;
    return sizeof(struct outer) + sizeof(struct inner) + o.a + o.in.c + o.in.l + o.b;
}
