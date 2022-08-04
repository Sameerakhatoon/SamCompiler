# ch90 - implementing tenaries

`cond ? a : b` parses as a NODE_TYPE_EXPRESSION with op `"?"`, whose
right side is a NODE_TYPE_TENARY carrying `true_node` and
`false_node`. The book preserves the spelling "tenary" everywhere
(not "ternary"); we match that verbatim.

What landed:

- `struct node_tenary { true_node, false_node }` added to the node
  payload union.
- `make_tenary_node(true, false)` in node.c.
- `parse_for_tenary(history)`: pop the condition that's already on
  the stack, eat `?`, parse the true result, eat `:`, parse the false
  result, build a tenary node, wrap the whole thing in an EXPRESSION
  with op="?".
- `parse_exp` dispatch: `?` -> parse_for_tenary.

No new dedicated test; covered by existing expression tests once the
shape is wired up.
