int f(int x){
    switch (x) {
        case -1: return 10;
        case 'a' + 1: return 20;
        case 1 << 4: return 30;
        case (2 + 3) * 4: return 40;
        case sizeof(int) * 100: return 50;
        default: return 60;
    }
}
int main(void){ return (f(-1) + f('b') + f(16) + f(20) + f(400) + f(7)) / 2; }
