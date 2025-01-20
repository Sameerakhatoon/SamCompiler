/* A parameter declared as an array is a pointer (C11 6.7.6.3p7). */
typedef char name_t[16];

static long first(long* b){ return b[1]; }
static long second(long b[]){ return b[1] + (long)sizeof(b); }
static long third(long b[4]){ b[2] = 7; b++; return b[0]; }
static int length(name_t name){
    int n = 0;
    while(name[n]){
        n++;
    }
    return n + (int)sizeof(name);
}
static int pick(const char* words[], int i){ return words[i][0]; }

int main(void){
    long local[4];
    name_t name = "abc";
    const char* words[] = { "x", "yes" };
    local[0] = 10;
    local[1] = 20;
    if(first(local) != 20) return 2;
    if(second(local) != 20 + (long)sizeof(long*)) return 3;
    if(third(local) != 20 || local[2] != 7) return 4;
    if(length(name) != 3 + (int)sizeof(char*)) return 5;
    if(pick(words, 1) != 'y') return 6;
    return 0;
}
