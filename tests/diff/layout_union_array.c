union num { int i; unsigned char b[4]; };
int main(void){
    union num u;
    u.i = 0x01020304;
    return u.b[0] + u.b[3] * 2 + sizeof(union num);
}
