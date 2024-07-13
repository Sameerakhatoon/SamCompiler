// EXPECT: 1 2 3 4 5 6 7 45
// x86_64 calls with more than six arguments put the rest on the
// stack, 16-byte aligned (the shell's date printf has seven). Eight
// is the most the aarch64 and riscv64 backends pass.
int printf(const char* fmt, ...);
long f8(long a, long b, long c, long d, long e, long f, long g, long h){ return a + b + c + d + e + f + g + h; }
int main(){
    long x = 9;
    printf("%d %d %d %d %d %d %d ", 1, 2, 3, 4, 5, 6, 7);
    printf("%d\n", f8(1, 2, 3, 4, 5, 6, 7, 8) + x);
    return 0;
}
