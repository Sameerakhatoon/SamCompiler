// EXPECTED EXIT: 120
//
// Exercises: recursive function calls, if statement, arithmetic,
// function args, return values from non-trivial call chains.
// Computes 5! = 120 recursively. Each call pushes its own stack
// frame; the codegen has to wire prologue / epilogue correctly
// for arbitrary recursion depth.

int factorial(int n)
{
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main()
{
    return factorial(5);
}
