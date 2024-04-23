# ch30 - dealing with precedence in expressions (part 2)

Wires the ch29 precedence table into the parser so freshly-built
expression subtrees get reordered for operator precedence and
associativity.

What landed in `parser.c`:

- `parser_get_precedence_for_operator(op, &group_out)` - linear scan
  of `op_precedence`; returns group index (low = high precedence) and
  the group pointer.
- `parser_left_op_has_priority(op_left, op_right)` - the comparison.
  Right-associative ops don't claim priority; identical spellings
  don't either; otherwise we compare group indices.
- `parser_node_shift_children_left(node)` - the actual rotation. For
  `node = a OPl (b OPr c)`, rebuild it as `(a OPl b) OPr c` by
  reusing the same node slot with swapped pointers and op.
- `parser_reorder_expression(&node)` - recursive entry called from
  `parse_exp_normal` after a NODE_TYPE_EXPRESSION is built.

Type defs `TOTAL_OPERATOR_GROUPS`, `MAX_OPERATORS_IN_GROUP`, the
associativity enum, and the struct moved out of `expressionable.c`
into `compiler.h` so the parser can extern them. `expressionable.c`
keeps only the table definition.

Smoke test (`tests/26-precedence-reorder.sh`) feeds `1+2*3` and
asserts the AST root is `+` with left=1 and right being the
expression `2*3` (not the other way around).
