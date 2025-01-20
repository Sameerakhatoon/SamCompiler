/* size_t from <stddef.h> is unsigned (C11 7.19). */
#include <stddef.h>
int main(void){
    size_t half = (size_t)-1 / 2;
    size_t zero = 0;
    if(half == 0 || half + half + 1 != (size_t)-1) return 1;
    if(zero - 1 < zero) return 2;
    if(sizeof(size_t) != sizeof(void*)) return 3;
    return 0;
}
