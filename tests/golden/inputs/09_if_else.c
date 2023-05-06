// EXPECTED EXIT: 2
int pick(int x)
{
    if(x == 1){
        return 10;
    } else if(x == 2){
        return 2;
    } else {
        return 30;
    }
    return 0;
}

int main()
{
    return pick(2);
}
