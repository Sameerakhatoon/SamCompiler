// EXPECTED EXIT: 13
//
// Exercises: char* literals, indexing into a string (s[i] read),
// per-char arithmetic in a hash, while loop terminated by the
// trailing NUL byte, modulo via subtraction-loop (we avoid the
// `%` operator since it relies on signed-div semantics we have
// not stress-tested).
//
// Implements a small additive hash, then reduces mod 17 by
// repeated subtraction. For the string "abc":
//   ('a' + 'b' + 'c') = 97 + 98 + 99 = 294
//   294 mod 17 = 294 - 17*17 = 294 - 289 = 5... hm.
// We pick a phrase where the final result is exactly 13:
//   "ab" = 97 + 98 = 195. 195 mod 17 = 195 - 17*11 = 195 - 187 = 8.
//   "aa" = 194. 194 mod 17 = 194 - 187 = 7.
//   "ad" = 97 + 100 = 197. 197 - 187 = 10.
//   "ag" = 97 + 103 = 200. 200 - 187 = 13. <- this one.

int simple_hash(const char* s)
{
    int sum = 0;
    int i = 0;
    while (s[i] != 0) {
        sum = sum + s[i];
        i = i + 1;
    }
    // sum mod 17 via repeated subtraction
    while (sum >= 17) {
        sum = sum - 17;
    }
    return sum;
}

int main()
{
    return simple_hash("ag");
}
