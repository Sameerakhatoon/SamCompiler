/* `continue` in a do ... while goes to the condition, not back to the
 * start of the body: a loop whose body always continues ran forever
 *. */
int main(void){
    int i = 0, n = 0;
    do {
        i++;
        if(i % 2) continue;
        n += i;
    } while(i < 10);
    int j = 0;
    do { j++; continue; } while(j < 5);
    int k = 0, m = 0;
    do {
        for(int q = 0; q < 3; q++){
            if(q == 1) continue;
            m++;
        }
        k++;
        if(k < 4) continue;
        m += 100;
    } while(k < 4);
    int z = 0;
    do { z++; if(z > 50) break; continue; } while(0);
    return n + j + k + m + z;
}
