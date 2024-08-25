# ch130 - implementing unaries

Unary expression parsing lands. `-x`, `!x`, `~x`, `*p`, `&p` all
build a `NODE_TYPE_UNARY` wrapping the operand.

What landed in `compiler.h`:
- `struct unary { op, operand, union { struct indirection { depth } } }`
  inside the node union.
- `HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL` so a `(` that
  appears as the right operand of an operator is parsed as a group,
  not a call.
- Forward decls for `is_unary_operator`, `op_is_indirection`,
  `make_unary_node`.

What landed in `helper.c`:
- `is_unary_operator(op)`: `-` `!` `~` `*` `&`.
- `op_is_indirection(op)`: `*`.

What landed in `node.c`:
- `make_unary_node(op, operand)`: builds the NODE_TYPE_UNARY.

What landed in `parser.c`:
- `parse_for_indirection_unary`: pull the contiguous `*` chain via
  `parser_get_pointer_depth`, parse the operand, build a single
  unary node with `indirection.depth` set.
- `parse_for_normal_unary`: consume the op token, parse operand,
  build the unary.
- `parse_for_unary`: dispatch by `op_is_indirection`. Normal unary
  also runs `parser_deal_with_additional_expression` so a trailing
  operator can keep building (e.g. `-x + 1`).
- `parse_exp_normal`:
  - empty left operand path: bail unless `is_unary_operator(op)`,
    then `parse_for_unary`.
  - right operand path: if next is `(`, dispatch to
    `parse_for_parentheses` with the new IS_NOT_A_FUNCTION_CALL
    flag; if next is a unary op, `parse_for_unary`; else fall back
    to the existing `parse_expressionable_for_op` path.

Test: `tests/78-unary-parse.sh` parses `int b = -5;` and asserts the
initializer is `NODE_TYPE_UNARY` with op `-` and a NUMBER(5)
operand.
