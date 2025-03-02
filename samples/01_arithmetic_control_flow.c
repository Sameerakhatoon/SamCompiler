// EXPECTED EXIT: 55  (sum of 1..10)
//
// Exercises: int variables, +=, for loop, while loop, return,
// arithmetic. Should compile end-to-end and produce a binary
// whose exit code equals 55.

int main()
{
    int total = 0;
    int i = 1;

    // for-loop arithmetic
    for (i = 1; i <= 5; i = i + 1) {
        total = total + i;
    }

    // while-loop arithmetic, picks up where the for-loop left off
    i = 6;
    while (i <= 10) {
        total = total + i;
        i = i + 1;
    }

    return total;
}
