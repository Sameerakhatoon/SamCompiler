// Stores through a dereferenced pointer expression: `*(p + 5) = 7`,
// `*(p + 1) += 2`, `(*(p + 2))++`, at each width. On x86 the store had
// no type and emitted `mov (null) [edx]`.
int main(void){
    char c[8] = { 0 };
    short s[8] = { 0 };
    int i[8] = { 0 };
    long l[8] = { 0 };
    char* pc = c;
    short* ps = s;
    int* pi = i;
    long* pl = l;
    int k = 2;
    *(pc + 5) = 100;
    *(pc + 5) += 27;
    (*(pc + k))++;
    *(ps + 5) = 300;
    *(ps + 1 + k) += 2;
    (*(ps + k))--;
    *((pi + 1) + 3) = 70000;
    *(pi + 1) += 2;
    (*(pi + k))++;
    *(pl + 5) = 9;
    *(pl + k * 2) -= 4;
    return (c[5] + c[2] + s[5] / 10 + s[3] + s[2] + i[4] / 1000 + i[1] + i[2] + (int)l[5] + (int)l[4]) % 127;
}
