typedef struct
{
    int a;
} T;
typedef struct S
*SP;
int main(void){ T t; t.a = 4; SP p = 0; return t.a + (p == 0); }
