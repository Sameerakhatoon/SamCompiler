struct pt { int x, y; };
struct shape { char tag; struct pt pts[3]; long area; };
struct shape g = { 'q', { {1, 2}, [2] = {5, 6} }, 99 };
int grid[2][3] = { {1, 2, 3}, {4, 5, 6} };
int flat[2][3] = { 1, 2, 3, 4 };
int m[][2] = { {1, 2}, {3, 4}, {5, 6} };
const char* names[] = { "ab", "cde", 0 };
union u { int i; char c[4]; } gu = { 0x01020304 };
union u gv = { .c = { 9, 8 } };
int main(void){
    struct shape s = { .area = 7, .pts[1].y = 3, .tag = 't' };
    int local[5] = { [3] = 4, 1, };
    char word[10] = "hey";
    long sum = g.tag + g.pts[0].x + g.pts[0].y + g.pts[1].x + g.pts[2].y + g.area;
    sum += grid[1][2] + flat[1][0] + flat[1][2] + m[2][1] + sizeof(m) / sizeof(m[0]);
    sum += names[1][2] + (names[2] == 0) + sizeof(names) / sizeof(names[0]);
    sum += gu.c[0] + gv.c[1] + gv.c[2];
    sum += s.area + s.pts[1].y + s.tag + s.pts[0].x + s.pts[2].y;
    sum += local[0] + local[4] + word[2] + word[3] + word[9];
    return sum % 256;
}
