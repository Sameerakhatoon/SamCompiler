// EXPECTED EXIT: 9
// NO RUN x86: the i386 backend pushes the hidden return pointer first
struct pt {
    int x;
    int y;
};

struct pt make(int x, int y)
{
    struct pt r;
    r.x = x;
    r.y = y;
    return r;
}

int main()
{
    struct pt a;
    a = make(2, 7);
    return a.x + a.y;
}
