// __func__ (C11 6.4.2.2) and gcc's __FUNCTION__ and
// __PRETTY_FUNCTION__, as glibc's assert.h uses them.
#include <stdio.h>
#include <string.h>

static const char* name_of_helper(void){
    return __func__;
}

static int length(void){
    return (int)sizeof(__func__);
}

int main(void){
    printf("%s %s %d\n", name_of_helper(), __func__, length());
    printf("%s %s %d\n", __FUNCTION__, __PRETTY_FUNCTION__, (int)strlen(__func__));
    return strcmp(__func__, "main");
}
