union num { int i; unsigned char b[4]; };
struct holder { int tag; union num n; };
int main(void){
    union num u;
    u.i = 0x01020304;
    struct holder h;
    h.tag = 2;
    h.n.i = 7;
    return u.b[0] + h.n.i + h.tag + sizeof(struct holder);
}
