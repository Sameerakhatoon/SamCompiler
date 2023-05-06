// EXPECTED EXIT: 13
struct pt {
    int x;
    int y;
};

struct pt g;

int main()
{
    g.x = 6;
    g.y = 7;
    return g.x + g.y;
}
