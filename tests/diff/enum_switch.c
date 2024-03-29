enum op { ADD, SUB = -3, MUL };
int apply(enum op o, int a, int b){
    switch (o) {
        case ADD: return a + b;
        case SUB: return a - b;
        case MUL: return a * b;
    }
    return 0;
}
int main(void){ return apply(ADD, 2, 3) + apply(SUB, 9, 4) * 10 + apply(MUL, 2, 2) + MUL * 0 - SUB; }
