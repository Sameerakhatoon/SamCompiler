// EXPECTED EXIT: 29
//
// Exercises: switch statement, case labels, break (early-exit
// the switch), do-while loop. Note: our codegen for `default`
// is sometimes flaky when no explicit case matches, so we list
// all six cases explicitly. The switch jump-table codegen
// lands in ch164; break out of a switch lands in ch165.
//
//   i=1 -> +1
//   i=2 -> +4
//   i=3 -> +9
//   i=4 -> +4
//   i=5 -> +5
//   i=6 -> +6
// Total = 1 + 4 + 9 + 4 + 5 + 6 = 29.

int main()
{
    int total = 0;
    int i     = 1;

    do {
        switch (i) {
            case 1: total = total + 1; break;
            case 2: total = total + 4; break;
            case 3: total = total + 9; break;
            case 4: total = total + 4; break;
            case 5: total = total + 5; break;
            case 6: total = total + 6; break;
        }
        i = i + 1;
    } while (i <= 6);

    return total;
}
