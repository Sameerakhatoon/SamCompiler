// EXPECTED EXIT: 19
struct pt {
    int x;
    int y;
};

struct pt a;
struct pt b;

int main()
{
    b.x = 8;
    b.y = 11;
    a = b;
    return a.x + a.y;
}
