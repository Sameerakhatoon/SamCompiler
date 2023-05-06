// EXPECTED EXIT: 65
union u {
    int i;
    char c;
};

union u g;

int main()
{
    g.i = 0x141;
    return g.c;
}
