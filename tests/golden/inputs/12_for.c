// EXPECTED EXIT: 30
int main()
{
    int s = 0;
    int i;
    for(i = 0; i < 5; i = i + 1){
        s = s + i * 3;
    }
    return s;
}
