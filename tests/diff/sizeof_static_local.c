// sizeof of a static local and of its members (SamOS stdlib itoa,
// ktest): the local is a renamed global.
struct carg { char argument[40]; struct carg* next; };
int take(char* p, int n){ return n; }
int f(void){ static char text[12]; return take(text, sizeof(text)); }
int g(void){ static int v; static long arr[3]; return sizeof v + sizeof(arr); }
int h(void){ static struct carg arg; arg.next = 0; return sizeof(arg.argument) + sizeof(arg) / 8 + (arg.next == 0); }
int k(void){ static int n = 5; n = n + 1; return n; }
int main(void){ k(); return f() + g() + h() + k(); }
