// EXPECTED EXIT: 12
int set(int* p, int v)
{
    *p = v;
    return 0;
}

int main()
{
    int x = 0;
    set(&x, 12);
    return x;
}
