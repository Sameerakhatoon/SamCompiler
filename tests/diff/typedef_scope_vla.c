// A variable-length array typedef: its size is evaluated
// where the typedef is (C11 6.7.8p3), so changing n afterwards changes
// nothing; arrays of it, sizeof of the type and of objects, and two
// dimensions.
static int one(int n){
    typedef int row[n];
    n = n + 10;
    row a;
    for(int i = 0; i < (int)(sizeof(a) / sizeof(int)); i++){
        a[i] = i + 1;
    }
    int total = 0;
    for(int i = 0; i < (int)(sizeof(row) / sizeof(int)); i++){
        total += a[i];
    }
    return total + (int)sizeof(row) + n;
}

static int rows(int n, int m){
    typedef long line[m];
    line grid[n];
    n = 0;
    m = 0;
    int k = 0;
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 4; j++){
            grid[i][j] = k++;
        }
    }
    return (int)grid[2][3] + (int)(sizeof(grid) / sizeof(long)) + (int)(sizeof(line) / sizeof(long));
}

static int two(int n){
    typedef char box[n][n + 1];
    n = 1;
    box b;
    b[1][2] = 7;
    return (int)sizeof(box) + (int)sizeof(b) + b[1][2];
}

static int loop(int rounds){
    int total = 0;
    for(int r = 1; r <= rounds; r++){
        typedef short cell[r];
        cell c;
        total += (int)sizeof(c);
    }
    return total;
}

int main(void){
    if(one(4) != 10 + 16 + 14) return 1;
    if(rows(3, 4) != 11 + 12 + 4) return 2;
    if(two(3) != 12 + 12 + 7) return 3;
    if(loop(5) != 2 * (1 + 2 + 3 + 4 + 5)) return 4;
    return 0;
}
