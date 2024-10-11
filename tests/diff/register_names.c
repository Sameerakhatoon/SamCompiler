// Globals and functions named like registers or assembler keywords on
// one of the targets: NASM (gs, eax, r8, r12, byte, rel, wrt), aarch64
// (x1, w2, sp, lr, xzr) and riscv64 (t0, a0, s1, ra, zero, fp).
int gs = 1;
int eax = 2;
static int byte = 3;
int t0 = 4;
int a0[2] = {5, 6};
long sp = 7;
int x1 = 8;
static int w2 = 9;
int zero;
int* fp = &t0;

int r8(int v){ return v + gs; }
int r12(int v){ return v * eax; }
static int rel(int v){ return v - byte; }
int ra(int v){ return v + t0 + a0[1]; }
int lr(int v){ return v + (int)sp + x1; }
static int s1(int v){ return v + w2; }
int xzr(void){ return zero; }
int wrt(int (*f)(int), int v){ return f(v); }

int main(void){
    int total = 0;
    total += r8(10);              // 11
    total += r12(3);              // 6
    total += rel(10);             // 7
    total += ra(1);               // 11
    total += lr(1);               // 16
    total += s1(1);               // 10
    total += xzr();               // 0
    total += wrt(r8, 1);          // 2
    total += wrt(s1, 0);          // 9
    gs = 20; t0 = 30; sp = 40; zero = 1;
    total += gs + *fp + (int)sp + xzr();  // 91
    return total - 100;           // 63
}
