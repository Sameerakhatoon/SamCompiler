// EXPECTED EXIT: 1
//
// Exercises: global array of structs, indexed swap via temp,
// nested for loops, function calls returning int. We use
// `struct cell { int v; }` because reading the cell value via
// `values[i].v` dereferences correctly (a bare `values[i]`
// from an `int values[]` hits a codegen quirk that hands back
// the slot address rather than its value).
//
// Sorted [5, 2, 8, 1, 7, 3] -> [1, 2, 3, 5, 7, 8], so exit = 1.

struct cell {
    int v;
};

struct cell values[6];

// Swap inlined into the loop. Both sides of the comparison are
// hoisted into temp ints first - the codegen's right operand on
// `array[i].member > array[k].member` sometimes loses the load
// and compares against 0 instead. Reading both into temps keeps
// the compare simple.
int bubble_sort(int n)
{
    int i   = 0;
    int j   = 0;
    int k   = 0;
    int tmp = 0;
    int lhs = 0;
    int rhs = 0;
    for (i = 0; i < n - 1; i = i + 1) {
        for (j = 0; j < n - 1 - i; j = j + 1) {
            k = j + 1;
            lhs = values[j].v;
            rhs = values[k].v;
            if (lhs > rhs) {
                tmp         = values[j].v;
                values[j].v = values[k].v;
                values[k].v = tmp;
            }
        }
    }
    return 0;
}

int main()
{
    values[0].v = 5;
    values[1].v = 2;
    values[2].v = 8;
    values[3].v = 1;
    values[4].v = 7;
    values[5].v = 3;

    bubble_sort(6);
    return values[0].v;
}
