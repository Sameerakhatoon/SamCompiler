// A struct parameter returned whole, changed first or not, small and
// big, and its result used straight away (x86 crashed on the first).
struct s4  { int a; };
struct s12 { int a, b, c; };
struct s40 { long a, b, c, d, e; };

struct s4 pass4(struct s4 s){ return s; }
struct s12 pass12(struct s12 s){ return s; }
struct s40 pass40(struct s40 s){ s.c = s.c + 1; return s; }
struct s12 second(int k, struct s12 s){ s.b = s.b * k; return s; }

int main(void){
    struct s4 a = { 9 };
    struct s12 b = { 1, 2, 3 };
    struct s40 f = { 1, 2, 3, 4, 5 };
    struct s4 a2 = pass4(a);
    struct s12 b2 = pass12(b);
    struct s40 f2 = pass40(f);
    struct s12 b3 = second(5, b2);
    int sum = a2.a + b2.a + 2 * b2.b + 3 * b2.c + (int)f2.c * 7 + (int)f.c + b3.b * 11;
    return sum;
}
