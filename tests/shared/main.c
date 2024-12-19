// The program linked against tests/shared/lib.c's shared object
// (tests/400-shared-library.sh). It changes the library's exported
// data, which the library must then see, and interposes lib_hook.
#include <stdio.h>
#include <string.h>

extern int counter;
extern int table[4];
int lib_get(void);
int lib_add(int v);
int lib_use_hook(void);
int lib_apply(int (*f)(int), int v);
typedef int (*intfn)(int);
intfn lib_helper_ptr(void);
const char* lib_name(void);
int lib_print(const char* what, int v);

int lib_hook(void){
    return 4;
}

static int square(int v){
    return v * v;
}

int main(void){
    int bad = 0;
    if(lib_get() != 13) bad |= 1;
    counter = 100;
    table[1] = 20;
    table[2] = 9;
    if(lib_get() != 103) bad |= 2;
    if(lib_add(5) != 35 || counter != 105) bad |= 4;
    if(lib_use_hook() != 49) bad |= 8;
    if(lib_apply(square, 6) != 39) bad |= 16;
    if(lib_helper_ptr()(7) != 21) bad |= 32;
    if(strcmp(lib_name(), "samlib") != 0) bad |= 64;
    lib_print("counter", counter);
    printf("bad=%d\n", bad);
    return bad;
}
