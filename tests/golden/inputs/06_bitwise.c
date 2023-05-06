// EXPECTED EXIT: 42
int main()
{
    int a = 0x5A;
    int lo = a & 0x0F;
    int hi = (a ^ 0xFF) & 0x30;
    return lo | hi;
}
