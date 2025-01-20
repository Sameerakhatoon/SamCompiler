/* Array typedefs: `typedef long buf_t[4];` in objects, members,
   parameters, arrays of them and sizeof. */
typedef long buf_t[4];
typedef int grid_t[2][3];
typedef struct pt { int x; int y; } pts_t[2];
typedef char name_t[8];

struct holder {
    buf_t b;
    int tail;
};

buf_t global_buf = { 1, 2, 3, 4 };
grid_t global_grid;

static long sum(buf_t b){
    long s = 0;
    for(int i = 0; i < 4; i++){
        s += b[i];
    }
    return s;
}

static int sum_grid(void){
    int s = 0;
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 3; j++){
            s += global_grid[i][j];
        }
    }
    return s;
}

int main(void){
    buf_t local = { 10, 20, 30, 40 };
    buf_t two[2];
    pts_t pts = { { 1, 2 }, { 3, 4 } };
    name_t names[3] = { "ab", "cd", "ef" };
    struct holder h;
    buf_t many[] = { { 1, 2, 3, 4 }, { 5 } };
    if(sizeof(buf_t) != 4 * sizeof(long)) return 1;
    if(sizeof(two) != 8 * sizeof(long)) return 2;
    if(sizeof(grid_t) != 6 * sizeof(int)) return 3;
    if(sizeof(struct holder) != 5 * sizeof(long)) return 4;
    if(sum(local) != 100) return 5;
    if(sum(global_buf) != 10) return 6;
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 4; j++){
            two[i][j] = i * 4 + j;
        }
    }
    if(sum(two[1]) != 4 + 5 + 6 + 7) return 7;
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 3; j++){
            global_grid[i][j] = i + j;
        }
    }
    if(sum_grid() != 9) return 8;
    if(pts[1].y != 4 || pts[0].x != 1) return 9;
    if(names[2][1] != 'f' || sizeof(names) != 24) return 10;
    h.b[3] = 7;
    h.tail = 9;
    if(h.b[3] + h.tail != 16) return 11;
    if(sizeof(int[3]) != 12) return 12;
    if(sizeof(many) != 8 * sizeof(long) || many[1][0] != 5 || many[0][3] != 4 || many[1][3] != 0) return 13;
    return 0;
}
