// tests/300-link-multi-file.sh: the second object. Its globals are
// exported (gs and t0 named like registers), shared is main.c's, and
// its static helper must not clash with main.c's.
int counter = 10;
int gs = 3;
int t0[2] = {4, 5};
int total = 77;
extern int shared;

static int helper(int v){ return v * 2; }

int bump(int by){
    counter = counter + helper(by) + shared;
    return counter;
}

int x1(int v){ return v + gs + t0[1]; }
