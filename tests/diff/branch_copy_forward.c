// a pointer a call returned, tested and then stored through.
// At -O1 the variable lives in a callee-saved register; the peephole's
// copy-forward folded `mv s1, a0; bnez s1, L` into `bnez a0, L` and
// dropped the move, so the store on the taken side went through a stale
// s1 (riscv64; SamOS's wget faulted on `*eol = 0`). Here the stale
// value is the caller's own pointer variable, so the store lands in the
// wrong buffer instead of faulting.
char line[16] = "ab\r\ncd";
char other[16] = "xyz";

char* find(char* s, char c){
    while(*s && *s != c){
        s++;
    }
    return *s ? s : 0;
}

int cut(char* h){
    char* eol = find(h, '\r');
    if(eol){
        *eol = 0;
    }
    return h[0];
}

int cut2(char* h){
    char* p = find(h, 'c');
    if(p != 0){
        *p = 'C';
    }
    return h[0];
}

int main(void){
    char* o = other;
    int r = cut(line);
    r += cut2(line + 3);
    // o is used after the calls, so it stays in its register across them.
    return (r == 'a' + '\n') + (line[2] == 0) * 2 + (line[4] == 'C') * 4 + (o[0] == 'x') * 8 + (o[1] == 'y') * 16;
}
