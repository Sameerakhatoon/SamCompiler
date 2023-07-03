// EXPECTED EXIT: 9
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
