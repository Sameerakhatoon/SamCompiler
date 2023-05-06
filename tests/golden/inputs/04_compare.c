// EXPECTED EXIT: 37
int main()
{
    int a = 3;
    int b = 5;
    int r = 0;
    if(a < b){ r = r + 1; }
    if(a > b){ r = r + 2; }
    if(a <= b){ r = r + 4; }
    if(a >= b){ r = r + 8; }
    if(a == b){ r = r + 16; }
    if(a != b){ r = r + 32; }
    return r;
}
