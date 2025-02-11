/* sizeof of a thread-local array is the array's size. On i386 the name
 * of one stands for its address worked out at run time, and sizeof gave
 * a pointer's size. */
#include <stdio.h>

struct rec { int a; char tag[6]; };

_Thread_local int counts[10];
__thread char name[13] = "thread-local";
_Thread_local long longs[5];
_Thread_local struct rec recs[4];
static _Thread_local short hidden[7];

static int fill(void){
    int total = 0;
    for(int i = 0; i < (int)(sizeof counts / sizeof counts[0]); i++){
        counts[i] = i * 3;
        total += counts[i];
    }
    return total;
}

int main(void){
    static _Thread_local unsigned char bytes[33];
    long check = 0;
    check += sizeof counts;
    check += sizeof(counts) * 2;
    check += sizeof name + sizeof(name[0]);
    check += sizeof longs / sizeof longs[0];
    check += sizeof(longs[1]) + sizeof(&longs[0]);
    check += sizeof recs;
    check += sizeof(hidden);
    check += sizeof bytes;
    check += sizeof(int[sizeof counts / sizeof counts[0]]);
    check += fill();
    bytes[sizeof bytes - 1] = 9;
    check += bytes[32] + name[sizeof name - 2];
    printf("%ld %zu %zu\n", check, sizeof counts, sizeof(name));
    return (int)(check & 0x7f);
}
