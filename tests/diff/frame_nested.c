/* Locals of a block inside an else, after a static local, get room in
   the frame: calls made while they live must not overwrite them. */
struct big { long a; long b; long c; long d; long e; long f; };

static long touch(long x){
    volatile long pad[8];
    for(int i = 0; i < 8; i++){
        pad[i] = x + i;
    }
    return pad[7];
}

static long run(int flag){
    long first = 1;
    if(flag){
        first = 2;
    } else {
        static int count = 0;
        struct big b = {};
        b.a = touch(count++);
        b.f = touch(5);
        return first + b.a + b.b + b.c + b.d + b.e + b.f;
    }
    return first;
}

int main(void){
    if(run(1) != 2) return 1;
    if(run(0) != 1 + 7 + 12) return 2;
    return 0;
}
