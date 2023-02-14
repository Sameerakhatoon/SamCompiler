// EXPECTED EXIT: 4
//
// Exercises: #include of a header we shipped under
// pc_includes/. The preprocessor walks
// compile_process->include_dirs, which by default lists
// ./pc_includes first. We define A and B there, sum to 4.

#include <values.h>

int main()
{
    return A + B;
}
