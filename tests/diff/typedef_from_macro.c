// A typedef keyword that comes out of a macro, as glibc's <bits/types.h>
// writes `#define __STD_TYPE __extension__ typedef`. Past the end of the
// macro's body the preprocessor looked at the next file token without
// taking it, so the typedef reader pushed that token until the compiler
// ran out of memory (9 GB).
#define T typedef
#define __STD_TYPE __extension__ typedef
#define __UQUAD_TYPE unsigned long int
#define __DEV_T_TYPE __UQUAD_TYPE
#define __S32_TYPE int

T int a_t;
__STD_TYPE __DEV_T_TYPE dev_t2;
__STD_TYPE __S32_TYPE pid_t2;
__STD_TYPE int b_t;
#undef __STD_TYPE

int main(void){
    a_t a = 3;
    dev_t2 d = 40;
    pid_t2 p = -2;
    b_t b = 7;
    return (int)(a + d + p + b + (int)sizeof(dev_t2));
}
