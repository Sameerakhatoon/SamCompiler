// Multi-dimensional variable-length arrays and pointers to arrays
//: runtime sizes in any dimension, sizeof of the array, a
// row and an element type, rows used as pointers, a pointer to a row
// stepped with ++ and +, the distance between two of them, parameters
// `int a[n][m]` and `int a[][4]`, struct elements, three dimensions,
// compound assignments and ++ on elements.
struct pt { int x, y; };

static int sum(int n, int m, int a[n][m]){
    int s = 0;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < m; j++){
            s += a[i][j] * (i + 1);
        }
    }
    return s;
}

static int fixed_rows(int extra, int a[][4]){
    return a[1][2] + extra;
}

static int fixed_both(int a[2][4]){
    return a[1][3];
}

static int pointer_param(int n, int (*rows)[n], int count){
    int s = 0;
    for(int i = 0; i < count; i++){
        s += rows[i][n - 1];
    }
    return s;
}

static int two_d(int n, int m){
    int a[n][m];
    for(int i = 0; i < n; i++){
        for(int j = 0; j < m; j++){
            a[i][j] = i * 10 + j;
        }
    }
    int* row = a[2];
    return a[2][3] + a[1][0] + row[1] + (int)sizeof(a) + (int)sizeof(a[1]) + (int)sizeof a[1][2]
        + sum(n, m, a);
}

static int mixed(int n){
    int a[n][4];
    int b[2][n];
    a[2][3] = 7;
    b[1][2] = 5;
    a[0][0] = 1;
    a[0][0] += 4;
    a[0][0]++;
    --b[1][2];
    return a[2][3] + b[1][2] + a[0][0] + (int)sizeof(b) + (int)sizeof(a[0]) + (int)sizeof(int[n][3]);
}

static int pointers(int n){
    int a[4][n];
    for(int i = 0; i < 4; i++){
        for(int j = 0; j < n; j++){
            a[i][j] = i * 3 + j;
        }
    }
    int (*p)[n] = a;
    p++;
    int t = p[0][2] + (*p)[1];
    p = p + 2;
    t += **p;
    int (*q)[n] = &a[1];
    t += (int)(p - q) * 100;
    t += (int)sizeof(*p) + pointer_param(n, a, 4);
    return t;
}

static long three_d(int n, int m, int k){
    long a[n][m][k];
    for(int i = 0; i < n; i++){
        for(int j = 0; j < m; j++){
            for(int l = 0; l < k; l++){
                a[i][j][l] = i * 100 + j * 10 + l;
            }
        }
    }
    a[1][2][3] += 5;
    a[0][1][2]++;
    long* plane = a[1][1];
    return a[1][2][3] + a[0][1][2] + plane[2] + (long)(sizeof(a[1]) / sizeof(long)) + (long)(sizeof(a[1][1]) / sizeof(long));
}

static int structs(int n){
    struct pt a[n][n];
    a[1][0].x = 4;
    a[1][0].y = 9;
    struct pt* q = &a[1][0];
    return q->y + a[1][0].x;
}

int main(void){
    int x[2][4] = { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } };
    long total = two_d(3, 5) + mixed(3) + pointers(3) + three_d(2, 3, 4) + structs(2)
        + fixed_rows(1, x) + fixed_both(x) + (int)sizeof(int[3]);
    return (int)(total % 127);
}
