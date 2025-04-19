// preprocessing numbers that are no constants (C11 6.4.8):
// `0f` in a macro that is never used and in a group #if skips, and
// numbers made by pasting (`2.5` ## `f`, `1` ## `e` ## `3`).
#include <stdio.h>
#define UNUSED 0f
#define SUFFIX(x) x##f
#define CAT3(a, b, c) a##b##c
#if 0
double skipped = 0f + 1.5e+10q + 0x1.8 + 08 + 1_000;
#endif
int main(void){
    float a = SUFFIX(2.5);
    double b = CAT3(1, e, 3);
    double c = 0x1p-3;
    printf("%g %g %g\n", a, b, c);
    return 0;
}
