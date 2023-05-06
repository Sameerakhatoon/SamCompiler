// EXPECTED EXIT: 8
// EXPECTED EXIT lp64: 16
int main()
{
    return sizeof(int*) + sizeof(long);
}
