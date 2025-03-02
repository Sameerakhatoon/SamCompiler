// EXPECTED EXIT: 29
//
// Exercises: switch statement, case labels, break (early-exit
// the switch), default case, do-while loop. The switch
// jump-table codegen lands in ch164; break out of a switch
// lands in ch165. We loop 1..6 and accumulate a per-value
// contribution chosen by the switch:
//   i=1 -> +1
//   i=2 -> +4
//   i=3 -> +9
//   i=4,5,6 -> default = +i
// Sum = 1 + 4 + 9 + 4 + 5 + 6 = 29.

int main()
{
    int total = 0;
    int i     = 1;

    do {
        switch (i) {
            case 1:
                total = total + 1;
                break;
            case 2:
                total = total + 4;
                break;
            case 3:
                total = total + 9;
                break;
            default:
                total = total + i;   // 4+5+6 = 15 across remaining iterations
                break;
        }
        i = i + 1;
    } while (i <= 6);

    return total;   // 1 + 4 + 9 + (4 + 5 + 6) = 29
}
