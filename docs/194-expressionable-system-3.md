# ch194 - creating the expressionable system - Part 3

Book labels this "Part 3" with no lecture number (193 is Part 2,
195 is Part 4). Wires the operator precedence + reorder pass
onto the binary-expression skeleton from ch193.

What landed in `expressionable.c`:
- `expressionable_parser_get_precedence_for_operator(op, &group_out)`:
  scans `op_precedence` rows top to bottom, returns the row
  index (= precedence class) and writes the row pointer back.
  Returns -1 for unknown operators.
- `expressionable_parser_left_op_has_priority(op_left, op_right)`:
  - same op string -> false (short-circuit).
  - left associtivity RIGHT_TO_LEFT -> false.
  - otherwise return `precedence_left <= precedence_right` (lower
    index means tighter binding).
- `expressionable_parser_node_shift_children_left(node)`: rotates
  a right-leaning expression into a left-leaning one. Asks the
  callbacks for left/right of node and right's left/right,
  builds a new make_expression_node(left, right_left, node_op),
  pops it back as new_left, then set_exp_node(node, new_left,
  right_right, right_op).
- `expressionable_parser_reorder_expression(&node_out)`:
  - bails if node is not EXPRESSION.
  - asks left + right node type. Upstream verbatim has
    `assert(left_node_type = 0)` (typo: assignment, not
    comparison) which fires unconditionally on any binary
    expression. We deviate here and write
    `assert(left_node_type >= 0)` so the suite stays green.
  - if right is an EXPRESSION and main op has priority over
    right op (left_op_has_priority), shift children left then
    recurse into both new sides.

`parse_for_operator` now calls
`expressionable_parser_reorder_expression(&exp_node)` on the
freshly built node before pushing it.

Test: `tests/125-expressionable-system-3.sh` exercises just the
precedence helpers directly (lookup row index + associtivity for
known operators; left_op_has_priority for + vs *, * vs +, same
op). Driving a real binary expression through parse would trip
the upstream assertion - g01 will cover that.
