#include "compiler.h"

// C operator precedence table. Rows are precedence classes, highest
// first. Each row also records associativity. The parser uses this to
// reorder freshly-built NODE_TYPE_EXPRESSION subtrees so e.g. 1+2*3
// becomes 1+(2*3) instead of (1+2)*3.

#define TOTAL_OPERATOR_GROUPS  14
#define MAX_OPERATORS_IN_GROUP 12

enum {
    ASSOCIATIVITY_LEFT_TO_RIGHT,
    ASSOCIATIVITY_RIGHT_TO_LEFT,
};

struct expressionable_op_precedence_group {
    char* operators[MAX_OPERATORS_IN_GROUP];
    int   associtivity;   // typo preserved from upstream
};

// Each entry is a NULL-terminated list of operator spellings sharing
// the same precedence class and associativity.
struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS] = {
    { .operators = { "++", "--", "()", "[]", "(", "[", ".", "->", 0 }, .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "*", "/", "%", 0 },                                .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "+", "-", 0 },                                     .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "<<", ">>", 0 },                                   .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "<", "<=", ">", ">=", 0 },                         .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "==", "!=", 0 },                                   .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "&", 0 },                                          .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "^", 0 },                                          .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "|", 0 },                                          .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "&&", 0 },                                         .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "||", 0 },                                         .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
    { .operators = { "?", ":", 0 },                                     .associtivity = ASSOCIATIVITY_RIGHT_TO_LEFT },
    { .operators = { "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=", 0 },
                                                                        .associtivity = ASSOCIATIVITY_RIGHT_TO_LEFT },
    { .operators = { ",", 0 },                                          .associtivity = ASSOCIATIVITY_LEFT_TO_RIGHT },
};
