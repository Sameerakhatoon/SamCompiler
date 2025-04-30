/* _Alignof of types, <stdalign.h>'s alignof, and gcc's __alignof__ and
 * _Alignof of an expression (the preferred alignment: 8 for a double on
 * i386, where _Alignof(double) is 4). None of them was supported
 *. long long is left out: it is 4 bytes on i386
 * here. */
#include <stdalign.h>

struct s { char c; double d; };
struct t { char c; short h; };
struct n { struct t inner; char tail; };
union u { char c; int i; long l; };
enum e { E0, E1 };

static unsigned h;
static void mix(unsigned long v){ h = h * 31 + (unsigned)v; }

int main(void){
    double dv = 0; struct s sv; int arr[3]; long double ld = 0; char c = 0; short sh[2];
    (void)sv; (void)arr; (void)sh;
    mix(_Alignof(char)); mix(_Alignof(short)); mix(_Alignof(int)); mix(_Alignof(long));
    mix(_Alignof(float)); mix(_Alignof(double)); mix(_Alignof(long double)); mix(_Alignof(_Bool));
    mix(_Alignof(void*)); mix(_Alignof(char*)); mix(_Alignof(int (*)(int)));
    mix(alignof(struct s)); mix(alignof(struct t)); mix(alignof(struct n)); mix(alignof(union u));
    mix(alignof(enum e)); mix(_Alignof(int[4])); mix(_Alignof(struct s[2])); mix(_Alignof(unsigned char));
    mix(_Alignof(dv)); mix(_Alignof(sv)); mix(_Alignof(arr)); mix(_Alignof(ld)); mix(_Alignof(c));
    mix(_Alignof(sh[1])); mix(_Alignof(sv.c));
    mix(__alignof__(double)); mix(__alignof__(int)); mix(__alignof__(sv.d)); mix(__alignof__(struct s));
    static int sized[_Alignof(short) + 1];
    mix(sizeof(sized));
    int r = _Alignof(int) == 4 && alignof(char) == 1 && __alignof_is_defined;
    return (h % 200) + r;
}
