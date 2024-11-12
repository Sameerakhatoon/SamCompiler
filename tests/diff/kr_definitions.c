// Old-style (K&R) function definitions: an identifier list, then the
// parameters' declarations in any order, several in one declaration,
// pointers, an array parameter, a function pointer, a struct, and a
// name with no declaration, which is an int.
struct point { int x, y; };

int add(a, b)
int a;
int b;
{
    return a + b;
}

long mix(count, text, scale)
char* text;
long scale;
{
    long total = 0;
    for(int i = 0; i < count; i++){
        total = total + text[i];
    }
    return total * scale;
}

int sum(values, n)
int n, values[];
{
    int total = 0;
    while(n-- > 0){
        total = total + *values++;
    }
    return total;
}

int apply(f, x)
int (*f)(int);
int x;
{
    return f(x);
}

int twice(x) int x; { return 2 * x; }

int area(p, unused)
struct point p;
{
    return p.x * p.y + unused;
}

int none()
{
    return 3;
}

int main(void){
    int v[4] = { 1, 2, 3, 4 };
    struct point p = { 3, 5 };
    int r = add(2, 3);
    r = r + (int)mix(3, "abc", 2L) - 588;
    r = r + sum(v, 4);
    r = r + apply(twice, 7);
    r = r + area(p, 1);
    return r + none();
}
