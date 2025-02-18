// built by gcc; calls the variadic functions of va_sam.c
// (built by SamCompiler) with structs of long doubles, and the same
// functions built by gcc (va_sam.c renamed), and compares the results.
#include <stdio.h>

struct q1 { long double a; };
struct q2 { long double a, b; };
struct q3 { long double a, b, c; };
struct q4 { long double a, b, c, d; };
struct qi { long double a; int n; };

long double sam_q(int n, ...);
long double sam_q4(int n, ...);
long double sam_qi(int n, ...);
long double gcc_q(int n, ...);
long double gcc_q4(int n, ...);
long double gcc_qi(int n, ...);
long double sam_qx(int n, ...);
long double gcc_qx(int n, ...);
long double sam_call(int k);
long double gcc_call(int k);

static int bad;

static void check(const char* what, long double sam, long double gcc){
    if(sam != gcc){
        printf("FAIL %s: sam %Lg, gcc %Lg\n", what, sam, gcc);
        bad++;
    }
}

int main(void){
    struct q1 a = { 1.25L }, a2 = { -7.5L };
    struct q2 b = { 2.5L, 1.0L / 3 }, b2 = { 11, 0.125L };
    struct q3 c = { 3, 4, 5 }, c2 = { -1.0L / 7, 9, 1e30L };
    struct q4 d = { 1, 2, 3, 4 }, d2 = { 0.5L, -0.25L, 8, 16 };
    struct qi e = { 1.5L, 3 }, e2 = { -2.25L, -7 };
    check("q one", sam_q(1, a, b, 0.5, c, 6.0L), gcc_q(1, a, b, 0.5, c, 6.0L));
    check("q two", sam_q(2, a, b, 0.5, c, 6.0L, a2, b2, -3.0, c2, 1.0L / 3),
                   gcc_q(2, a, b, 0.5, c, 6.0L, a2, b2, -3.0, c2, 1.0L / 3));
    check("q three", sam_q(3, a, b, 0.5, c, 6.0L, a2, b2, -3.0, c2, 1.0L / 3, a, b2, 2.0, c, -9.0L),
                     gcc_q(3, a, b, 0.5, c, 6.0L, a2, b2, -3.0, c2, 1.0L / 3, a, b2, 2.0, c, -9.0L));
    check("q4 one", sam_q4(1, d, 5), gcc_q4(1, d, 5));
    check("q4 three", sam_q4(3, d, 5, d2, -6, d, 7), gcc_q4(3, d, 5, d2, -6, d, 7));
    check("qi", sam_qi(3, e, b, e2, b2, e, b), gcc_qi(3, e, b, e2, b2, e, b));
    check("qx", sam_qx(1, 2, 3, 4, 5, 6, 7, 8, d, d2, a2, 9, b, 1.0L / 3, c2),
                gcc_qx(1, 2, 3, 4, 5, 6, 7, 8, d, d2, a2, 9, b, 1.0L / 3, c2));
    for(int k = 0; k < 3; k++){
        char what[16];
        snprintf(what, sizeof what, "call %d", k);
        check(what, sam_call(k), gcc_call(k));
    }
    if(!bad){
        printf("ok\n");
    }
    return bad != 0;
}
