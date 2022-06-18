#include "compiler.h"

// C operator precedence table. Rows are precedence classes, highest
// first. Each row also records associativity. The parser uses this to
// reorder freshly-built NODE_TYPE_EXPRESSION subtrees so e.g. 1+2*3
// becomes 1+(2*3) instead of (1+2)*3.
//
// ch30 moved the type defs (TOTAL_OPERATOR_GROUPS, struct, enum) into
// compiler.h so parser.c can extern this table and walk it. The table
// itself stays here.
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
