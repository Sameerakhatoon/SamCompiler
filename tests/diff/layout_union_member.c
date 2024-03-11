union num { int i; char c; };
struct holder { char tag; union num n; char z; };
int main(void){
    struct holder h;
    h.tag = 2; h.n.i = 7; h.z = 5;
    return sizeof(struct holder) * 4 + h.n.i + h.tag + h.z;
}
