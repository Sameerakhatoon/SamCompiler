// A function returning a function pointer, written without a typedef:
// `int (*pick(int))(int, int)`, its prototype, a call through the
// result, and the result stored and passed on.
int add(int a, int b){ return a + b; }
int sub(int a, int b){ return a - b; }
int mul(int a, int b){ return a * b; }

int (*pick(int op))(int, int);

int (*pick(int op))(int, int){
    if(op == 0){
        return add;
    }
    return op == 1 ? sub : mul;
}

int (*table[3])(int, int) = { add, sub, mul };

int (*table_entry(int i))(int, int){
    return table[i];
}

int apply(int (*f)(int, int), int x, int y){
    return f(x, y);
}

int main(void){
    int (*f)(int, int) = pick(1);
    int r = f(10, 3);
    r = r + pick(2)(4, 5) * 2;
    r = r + apply(pick(0), 6, 7);
    r = r + table_entry(2)(3, 3);
    f = table_entry(1);
    return r + f(1, 9);
}
