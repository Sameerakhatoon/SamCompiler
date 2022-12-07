# ch196 - creating the expressionable system - Part 5

Wires unary operator parsing into the expressionable loop.
This covers prefix unaries (`!x`, `~x`, `-x`) and pointer
indirection (`*p`, `**pp`, ...).

What landed in `expressionable.c`:
- `expressionable_token_next_is_operator(op)`: peek + compare,
  doesn't consume.
- `expressionable_get_pointer_depth`: consumes consecutive `*`
  operators, returns the count.
- `expressionable_parse_for_indirection_unary`: walks the
  pointer depth, recurses parse for the operand, pops it, calls
  `make_unary_indirection_node(depth, operand)`.
- `expressionable_parse_for_normal_unary`: consumes the unary
  op token, recurses parse for the operand, pops it, calls
  `make_unary_node(op, operand)`.
- `expressionable_parse_unary`: dispatches on
  `op_is_indirection(op)`. Falls through to
  `deal_with_additional_expression` for the normal-unary case
  so a trailing operator (e.g. `! a + b`) keeps parsing.

Wired:
- `parse_for_operator`'s "no left operand" branch now requires
  `is_unary_operator(op)` and calls `parse_unary` instead of
  the old `#warning "deal with a unary"` stub.
- `parse_for_operator`'s "follow-up operator after operator"
  branch now calls `parse_unary` instead of the
  `#warning "parse the unary"` stub.

Test: `tests/127-expressionable-unary.sh` feeds two minimal
inputs: `! 1` (confirms `make_unary_node` fires with op `!`)
and `* 2` (confirms `make_unary_indirection_node` fires with
depth 1).
