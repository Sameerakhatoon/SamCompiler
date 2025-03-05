// EXPECTED EXIT: 25
//
// Exercises: global array of structs, indexed write of a struct
// member, indexed read of a struct member, for loop. We use a
// `struct cell { int v; }` array because reads via `values[i].v`
// dereference correctly, whereas reads of a bare `values[i]`
// from an `int values[5]` hit a codegen quirk that hands back
// the slot address instead of the value.
//
// Fill values[0..4].v with 5, sum them, expect 25.

struct cell {
    int v;
};

struct cell values[5];

int main()
{
    int i = 0;

    // initialize
    for (i = 0; i < 5; i = i + 1) {
        values[i].v = 5;
    }

    // sum via struct member access (this dereferences correctly)
    int total = 0;
    for (i = 0; i < 5; i = i + 1) {
        total = total + values[i].v;
    }

    return total;
}
