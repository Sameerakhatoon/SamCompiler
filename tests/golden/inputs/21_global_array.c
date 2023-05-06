// EXPECTED EXIT: 9
int g[4];

int main()
{
    g[1] = 4;
    g[3] = 5;
    int a = g[1];
    int b = g[3];
    return a + b;
}
