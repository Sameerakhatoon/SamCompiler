// dead branches, unreachable statements and unused static
// functions go under -O1, but code a label, case or goto reaches stays.
static int unused_one(int x){ return x + 1; }
static int unused_two(int x){ return unused_one(x) * 2; }
static int used(int x){ return x * 3; }
static int called_by_table(int x){ return x - 1; }
int (*table[1])(int) = { called_by_table };

int counter;

static int bump(void){ return ++counter; }

// A goto into a dead branch is in tests/550 (on x86 a goto label
// inside a block breaks NASM's local labels, at -O0 too).
int labels(int n){
    int r = n > 100 ? 50 : 0;
    while(0){
        r += 1000;
    }
    for(; 0; ){
        r += 1000;
    }
    do {
        r += 1;
    } while(0);
    if(1){
        r += 2;
    } else {
        r += 1000;
    }
    if(0){
        r += 1000;
    } else if(1){
        r += 4;
    } else {
        r += 1000;
    }
    return r;
}

int sw(int k){
    int r = 0;
    switch(k){
        case 1:
            return 10;
            r += 1000;
        case 2:
            r += 20;
            break;
            r += 1000;
        default:
            if(0){
        case 3:
                r += 30;
            }
            break;
    }
    return r;
}

int after_return(void){
    int x = 5;
    return x;
    x = 9;
    bump();
}

int loops(void){
    int r = 0;
    for(int i = 0; i < 5; i++){
        if(i == 1){
            continue;
            r += 1000;
        }
        if(i == 3){
            break;
            r += 1000;
        }
        r += i;
    }
    do {
        r += 100;
        if(r < 0){
            break;
        }
    } while(0);
    return r;
}

int main(void){
    int s = 0;
    s += labels(1) + labels(200);      // 7 + 57
    s += sw(1) + sw(2) + sw(3) + sw(4); // 10 + 20 + 30 + 0
    s += after_return();               // 5
    s += loops();                      // 0 + 2 + 100
    s += used(2) + table[0](5);        // 6 + 4
    s += bump() + (0 && bump()) + (1 || bump()) + counter; // 1 + 0 + 1 + 1
    return s & 0x7f;
}
