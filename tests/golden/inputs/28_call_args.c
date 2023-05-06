// EXPECTED EXIT: 26
int f(int a, int b, int c)
{
    return a * 100 + b * 10 + c - 300;
}

int main()
{
    return f(3, 2, 6);
}
