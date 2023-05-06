// EXPECTED EXIT: 9
int main()
{
    int i = 0;
    int s = 0;
    while(i < 100){
        i = i + 1;
        if(i == 2){
            continue;
        }
        if(i > 4){
            break;
        }
        s = s + i;
    }
    return s + 1;
}
