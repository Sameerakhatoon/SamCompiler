// EXPECT: 1 2 3 0 x u 2 1 1
// brace initializers: a global array, a global array of structs
// with strings (its length taken from the list), a local `= {0}`.
int printf(const char* fmt, ...);
struct p { const char* a; const char* b; int n; };
int g[4] = {1, 2, 3};
struct p ps[] = { {"x", "u", 5}, {"y", "v"} };
int main(){
    struct p z = {0};
    printf("%d %d %d %d ", g[0], g[1], g[2], g[3]);
    printf("%s %s %d %d %d\n", ps[0].a, ps[0].b, (int)(sizeof(ps) / sizeof(ps[0])), z.a == 0, ps[1].n == 0);
    return 0;
}
