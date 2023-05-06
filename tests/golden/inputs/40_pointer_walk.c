// EXPECTED EXIT: 6
int main()
{
    const char* s = "abcdefg";
    const char* p = s;
    int n = 0;
    while(*p != 'g'){
        p = p + 1;
        n = n + 1;
    }
    return n;
}
