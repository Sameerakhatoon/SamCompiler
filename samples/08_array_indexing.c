// EXPECTED EXIT: 25
//
// Exercises: stack-local array declaration with size 5, array
// indexing via [], assignment-into-index, sum over the array.
// 5*5 = 25 because we fill the array with 5s and add them.

int main()
{
    int values[5];
    int i = 0;

    // initialize
    for (i = 0; i < 5; i = i + 1) {
        values[i] = 5;
    }

    // sum
    int total = 0;
    for (i = 0; i < 5; i = i + 1) {
        total = total + values[i];
    }

    return total;
}
