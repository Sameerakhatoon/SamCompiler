// EXPECT: 64 40 8 16
// a struct after a global int: its size counted padding after the
// global (sizeof(struct cpu_stat) was 72 in ktest).
int printf(const char* fmt, ...);
static int counter;
struct cpu_stat {
    unsigned long index, apic_id, ticks_busy, ticks_idle;
    unsigned long tasks_entered, steals, ipi_resched, ipi_tlb;
};
struct mixed { char x; unsigned long a, b, c; int d; struct { char q; } in; };
int main(){
    static struct cpu_stat a[4];
    printf("%d %d %d %d\n", (int)sizeof(struct cpu_stat), (int)sizeof(struct mixed),
           (int)((char*)&a[1].apic_id - (char*)&a[1]), (int)((char*)&a[2] - (char*)&a[1]) / 4);
    return counter;
}
