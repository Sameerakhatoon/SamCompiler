// EXPECTED EXIT: 4
//
// Exercises: char* indexing as a stream, state-machine-style
// scanner that detects word boundaries on spaces, struct +
// global counter, char range check via < / >, multiple cases
// in a switch.
//
// Counts the number of "words" (whitespace-delimited runs of
// non-space chars) in a hardcoded string. "one two three four"
// has four words.

struct scanner {
    const char* src;
    int         pos;
    int         word_count;
    int         in_word;
};

struct scanner sc;

int is_space(int c)
{
    return c == 32;   // ASCII ' '
}

// returns 0 always; we never look at the result. Returns int so
// the void-return codegen quirk doesn't bite when this is called
// as a statement.
int scan_step()
{
    int c = sc.src[sc.pos];
    if (c == 0) {
        return 0;
    }

    // Use explicit cases (not default) since default-case codegen
    // sometimes skips when an explicit case isn't matched.
    switch (is_space(c)) {
        case 1:
            // whitespace -> close any open word
            sc.in_word = 0;
            break;

        case 0:
            // non-space -> if we weren't in a word, this is a new word
            if (sc.in_word == 0) {
                sc.word_count = sc.word_count + 1;
                sc.in_word = 1;
            }
            break;
    }

    sc.pos = sc.pos + 1;
    return 0;
}

int main()
{
    sc.src = "one two three four";
    sc.pos = 0;
    sc.word_count = 0;
    sc.in_word = 0;

    while (sc.src[sc.pos] != 0) {
        scan_step();
    }

    return sc.word_count;
}
