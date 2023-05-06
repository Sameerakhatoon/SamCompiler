// EXPECTED EXIT: 30
int classify(int x)
{
    int r = 0;
    switch(x){
        case 1:
            r = 10;
            break;
        case 2:
            r = 20;
            break;
        default:
            r = 3;
            break;
    }
    return r;
}

int main()
{
    int a = classify(2);
    int b = classify(1);
    return a + b;
}
