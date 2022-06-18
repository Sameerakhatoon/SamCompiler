# ch29 - dealing with precedence in expressions (part 1)

New file `expressionable.c` holds the C operator precedence table the
parser will use to reorder freshly-built `NODE_TYPE_EXPRESSION`
subtrees so e.g. `1+2*3` becomes `1+(2*3)` instead of `(1+2)*3`.

Shape:

- `TOTAL_OPERATOR_GROUPS = 14` rows, highest precedence first.
- `MAX_OPERATORS_IN_GROUP = 12` per row.
- Each row is a NULL-terminated list of operator spellings plus an
  associativity flag.
- `ASSOCIATIVITY_LEFT_TO_RIGHT` / `_RIGHT_TO_LEFT`.

Group highlights:
- group 0: `++ -- () [] . ->` - postfix / member access.
- group 1: `* / %`.
- group 2: `+ -`.
- group 3-10: shifts, comparisons, bitwise, logical.
- group 11: ternary `? :` (right-to-left).
- group 12: assignment `= += -= ...` (right-to-left).
- group 13: comma.

The "associtivity" typo is preserved from the book.

ch30 (part 2) wires the table into the parser via
`parser_get_precedence_for_operator`, `parser_left_op_has_priority`,
and `parser_reorder_expression`.

Smoke test (`tests/25-precedence-table.sh`) confirms the table links
and that `*` is in group 1 (higher precedence) while `+` is in group
2 (lower).
