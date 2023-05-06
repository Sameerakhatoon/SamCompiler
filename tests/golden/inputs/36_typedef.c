// EXPECTED EXIT: 33
typedef int num_t;

struct pair {
    num_t a;
    num_t b;
};

typedef struct pair pair_t;

int main()
{
    pair_t p;
    p.a = 30;
    p.b = 3;
    return p.a + p.b;
}
