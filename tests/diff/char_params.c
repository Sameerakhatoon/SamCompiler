/* Parameters after a char or short one: every argument takes a whole
   slot, on x86 too. */
static int pick(char a, char b, short c, int d){ return a * 1000 + b * 100 + c * 10 + d; }
static int same(char a, char b){ return a == b; }
int main(void){
    if(pick(1, 2, 3, 4) != 1234) return 1;
    if(!same(7, 7) || same(7, 8)) return 2;
    return 0;
}
