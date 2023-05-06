// EXPECTED EXIT: 11
struct pt {
    int x;
    int y;
};

int main()
{
    struct pt p;
    p.x = 4;
    p.y = 7;
    return p.x + p.y;
}
