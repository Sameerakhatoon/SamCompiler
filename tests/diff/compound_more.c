struct pt { int x, y; };
int sum(int* a, int n){ int s = 0; int i; for (i = 0; i < n; i++) s += a[i]; return s; }
int area(struct pt* p){ return p->x * p->y; }
int* gp = (int[]){ 7, 8, 9 };
int main(void){
    int total = sum((int[]){ 1, 2, 3, 4 }, 4);
    total += area(&(struct pt){ .y = 5, .x = 3 });
    if (total > 0) total += (int[3]){ [2] = 6 }[2];
    return total + gp[2];
}
