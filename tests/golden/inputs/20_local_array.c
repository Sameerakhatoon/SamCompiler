// EXPECTED EXIT: 6
int main()
{
    int a[3];
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    int x = a[0];
    int y = a[1];
    int z = a[2];
    return x + y + z;
}
