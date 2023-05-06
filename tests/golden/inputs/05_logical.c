// EXPECTED EXIT: 6
int main()
{
    int a = 1;
    int b = 0;
    int x = a && b;
    int y = a || b;
    int z = !b;
    return x + y * 2 + z * 4;
}
