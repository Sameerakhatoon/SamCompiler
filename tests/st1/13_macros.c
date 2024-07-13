// EXPECT: renamed 3
// an object-like macro followed by `(` is only replaced:
// `#define main shell_main` before `int main(int argc, ...)`.
int printf(const char* fmt, ...);
#define work renamed_work
int work(int a){ return a + 1; }
#undef work
int main(){ printf("renamed %d\n", renamed_work(2)); return 0; }
