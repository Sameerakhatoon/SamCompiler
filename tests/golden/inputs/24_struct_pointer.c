// EXPECTED EXIT: 15
struct pt {
    int x;
    int y;
};

int sum(struct pt* p)
{
    return p->x + p->y;
}

int main()
{
    struct pt a;
    a.x = 5;
    a.y = 10;
    return sum(&a);
}
