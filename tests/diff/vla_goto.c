// A goto out of a block with a variable-length array, and one that
// jumps back over the declaration of one, give the stack back
//: each loop runs 100000 times and would take hundreds of
// megabytes of stack otherwise.
static int leave(int rounds){
    int t = 0, r = 0;
top:
    if(r >= rounds){
        return t;
    }
    {
        int n = 50 + r % 3;
        int a[n][n];
        a[n - 1][n - 1] = r;
        t += a[n - 1][n - 1] & 1;
        r++;
        if(r % 2){
            goto top;
        }
    }
    {
        int m = 400;
        int b[m];
        b[0] = r;
        t += b[0] & 1;
        r++;
        goto top;
    }
}

static int back(int rounds){
    int t = 0, r = 0;
    {
        int keep = 8;
        int first[keep];
        first[0] = 1;
again:
        r++;
        int n = 300;
        int a[n][2];
        a[n - 1][1] = r;
        t += a[n - 1][1] & 1;
        if(r < rounds){
            goto again;
        }
        t += first[0];
    }
    return t;
}

int main(void){
    return (leave(100000) + back(100000)) % 127;
}
