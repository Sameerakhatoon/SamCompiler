int main(void){
    int x = 1;
    { int x = 5; { int y = x * 2; x = y; } if (x == 10) { x = 11; } }
    { int z = 3; x += z; }
    return x;
}
