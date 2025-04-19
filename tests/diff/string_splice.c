// a backslash at the end of a line inside a string literal
// joins the next line (C11 5.1.1.2 phase 2), as glibc's <pthread.h> and
// <bits/math-vector.h> continue their messages.
#include <stdio.h>
#include <string.h>

static const char* message = "first half, \
second half";

int main(void){
    const char* inside = "a\
b\
c";
    printf("%s|%s|%d\n", message, inside, (int)strlen(inside));
    return 0;
}
