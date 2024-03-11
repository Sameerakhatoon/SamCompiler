int main(void){
    int i = 2;
    char c = "hello"[i];
    return c - 'a' + "xyz"[0] - 'x' + sizeof("abc");
}
