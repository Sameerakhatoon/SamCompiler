// EXPECTED EXIT: 17
int add(int a, int b)
{
    return a + b;
}

int twice(int a)
{
    return a * 2;
}

int main()
{
    return add(twice(5), add(3, 4));
}
