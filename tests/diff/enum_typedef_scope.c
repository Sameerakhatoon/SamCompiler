typedef enum { OFF, ON } state;
struct dev { state s; int ON_count; };
int main(void){
    struct dev d; d.s = ON; d.ON_count = 4;
    int total = d.s + d.ON_count;
    { enum { OFF = 10 }; total += OFF; }
    total += OFF;
    int ON = 7;
    return total + ON;
}
