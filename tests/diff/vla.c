// Variable-length arrays: a runtime size, sizeof at run time, elements
// read and written, passed to a function, several in one block, a VLA
// in a loop body that runs many times (the stack must not grow), with
// break and continue leaving it, and in nested blocks.
int sum(int* a, int n){
    int s = 0;
    for(int i = 0; i < n; i++){
        s = s + a[i];
    }
    return s;
}

long fill(int n){
    long a[n];
    char tag[n + 3];
    for(int i = 0; i < n; i++){
        a[i] = i * 3;
        tag[i] = (char)i;
    }
    tag[n + 2] = 5;
    return a[n - 1] + (long)(sizeof(a) / sizeof(long)) + (long)sizeof tag + tag[n + 2];
}

int loop(int rounds){
    int total = 0;
    for(int r = 0; r < rounds; r++){
        int size = 100 + r % 7;
        int buffer[size];
        for(int i = 0; i < size; i++){
            buffer[i] = i;
        }
        if(r % 5 == 3){
            continue;
        }
        if(r == rounds - 1){
            total = total + buffer[size - 1];
            break;
        }
        total = total + (int)(sizeof(buffer) / sizeof(buffer[0])) - size;
    }
    return total;
}

int nested(int n){
    int outer[n];
    outer[0] = 7;
    int k = 0;
    while(k < 3){
        int inner[n * 2];
        inner[2 * n - 1] = k;
        outer[0] = outer[0] + inner[2 * n - 1];
        k++;
    }
    {
        short more[n];
        more[n - 1] = 2;
        outer[n - 1] = more[n - 1];
    }
    return outer[0] * 10 + outer[n - 1];
}

int main(void){
    int n = 5;
    int v[n];
    for(int i = 0; i < n; i++){
        v[i] = i + 1;
    }
    if(sizeof(v) != 5 * sizeof(int)) return 1;
    if(sum(v, n) != 15) return 2;
    if(fill(4) != 9 + 4 + 7 + 5) return 3;
    if(loop(200000) != 99 + 199999 % 7) return 4;
    if(nested(3) != 102) return 5;
    return v[4] * 10 + n;
}
