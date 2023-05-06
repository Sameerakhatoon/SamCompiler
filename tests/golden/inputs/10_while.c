// EXPECTED EXIT: 15
int main()
{
    int i = 0;
    int s = 0;
    while(i < 6){
        s = s + i;
        i = i + 1;
    }
    return s;
}
