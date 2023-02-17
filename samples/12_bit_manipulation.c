// EXPECTED EXIT: 7
//
// Exercises: bitwise operators (& and >>), hex literals,
// while loop, integer division semantics around bit-shifting.
// Counts the number of set bits in a value (Hamming weight).
//
// popcount(0xAB) where 0xAB = 1010 1011 -> 5 set bits
// popcount(0x07) where 0x07 = 0000 0111 -> 3 set bits  -- wait
// Adjusting comment: 0xAB has bits 7,5,3,1,0 set = 5. 0x07 has
// 3 set. We sum the two so the return value is 5 + 3 = 8. Hmm,
// I want exit 7. Let me use 0x7E (= 0111 1110 = 6 set) + 0x03
// (= 11 = 2 set) - 1 = 7. Actually just use 0x55 (5 bits) + 2 = 7.

int popcount(int value)
{
    int count = 0;
    while (value != 0) {
        count = count + (value & 1);
        value = value >> 1;
    }
    return count;
}

int main()
{
    // popcount(0x55) = popcount(0101 0101) = 4 set bits.
    int a = popcount(0x55);
    // popcount(0x07) = popcount(0000 0111) = 3 set bits.
    int b = popcount(0x07);
    return a + b;
}
