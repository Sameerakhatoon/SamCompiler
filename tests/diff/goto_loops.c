int main(void){
    int i, j, found = 0;
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            if (i * j == 42) { found = i * 10 + j; goto done; }
        }
    }
done:
    ;
    int k = 0;
top:
    k++;
    switch (k) { case 1: goto top; case 2: goto top; default: break; }
    return found + k;
}
