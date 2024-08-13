typedef int (*binop)(int, int);
int add(int a, int b){ return a + b; }
int sub(int a, int b){ return a - b; }
int mul(int a, int b){ return a * b; }
struct calc { const char* name; binop op; };
struct calc table[3] = { { "add", add }, { "sub", sub }, { "mul", mul } };
int run(binop f, int x, int y){ return (*f)(x, y); }
binop pick(int i){ return table[i].op; }
int (*global_op)(int, int) = mul;
int main(void){
    int total = 0;
    int i;
    for (i = 0; i < 3; i++) total += table[i].op(6, 2);
    total += run(sub, 10, 3);
    binop f = pick(2);
    total += f(3, 3);
    total += global_op(2, 5);
    int (*local[2])(int, int) = { add, mul };
    total += local[1](4, 4);
    total += (f == mul) + (f != add);
    return total;
}
