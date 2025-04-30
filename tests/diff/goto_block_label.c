/* A goto label inside a block, with an if and a loop after it in the
 * same function: on x86 and x86_64 the label was a NASM non-local label,
 * so the `.if_N` labels after it were undefined and the assembler failed
 *. */
static int f(int n){
    int r = 0;
    if(n > 100){ goto inside; }
    if(n > 5){
    inside:
        r += 50;
        if(n > 200) r += 1;
        else r += 2;
    }
    while(n > 0){
        n--;
        if(n == 3) goto out;
        r++;
    }
out:
    return r;
}

static int g(int n){
    int r = 0;
    for(int i = 0; i < n; i++){
        if(i == 2) goto skip;
        r += i;
    skip:
        if(i & 1) r += 10;
    }
    return r;
}

int main(void){
    return f(0) + f(7) + f(150) + f(300) + g(5);
}
