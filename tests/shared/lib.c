// A shared library (tests/400-shared-library.sh).
// Exported data and functions, static ones, a string, libc calls and a
// symbol the program interposes.
#include <stdio.h>

int counter = 10;
int table[4] = {1, 2, 3, 4};
static int hidden = 3;
int* table_ptr = table;

static int helper(int v){
    return v * hidden;
}

int lib_get(void){
    return counter + hidden;
}

int lib_add(int v){
    counter += v;
    return helper(v) + table[1];
}

// The program defines its own lib_hook: the library's calls must reach
// that one, not this.
int lib_hook(void){
    return 1;
}

int lib_use_hook(void){
    return lib_hook() * 10 + table_ptr[2];
}

int lib_apply(int (*f)(int), int v){
    return f(v) + helper(1);
}

typedef int (*intfn)(int);

intfn lib_helper_ptr(void){
    return helper;
}

const char* lib_name(void){
    return "samlib";
}

int lib_print(const char* what, int v){
    char buf[64];
    snprintf(buf, sizeof(buf), "%s=%d", what, v);
    return printf("lib: %s\n", buf);
}
