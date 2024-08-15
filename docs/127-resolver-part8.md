# ch127 - creating the resolver - Part 8

Operator/node predicates and the first chunk of expression-walking.

What landed in `helper.c` (moved + new):
- `is_array_node` moved from `node.c` so the access / array / parens
  helpers live together.
- `is_access_operator(op)` / `is_access_node(node)` for `.` / `->`.
- `is_access_node_with_op(node, op)`: access node whose op matches.
- `is_array_operator(op)` / `is_array_node(node)`: `[]`.
- `is_parentheses_operator(op)` / `is_parentheses_node(node)`: `()`.

What landed in `resolver.c`:
- `resolver_follow_variable`: a NODE_TYPE_VARIABLE on the follow
  path is treated as an identifier whose name is `var.name`.
- `resolver_follow_struct_exp`: handles `a.b` / `a->b`.
  - Walks the left.
  - Picks up the resulting left entity to decide rule flags.
  - For `->`, sets `rule.left.flags =
    NO_MERGE_WITH_NEXT_ENTITY`; if the left is not a FUNCTION_CALL,
    sets `rule.right.flags = DO_INDIRECTION` (need to dereference
    the pointer before stepping into the field).
  - Pushes a RULE entity carrying those flags via
    `resolver_new_entity_for_rule`.
  - Walks the right.
- `resolver_follow_exp`: dispatch on EXPRESSION nodes; only the
  access path lands now.
- `resolver_follow_part_return_entity`: extended with VARIABLE and
  EXPRESSION cases.

Test: `tests/75-resolver-helpers.sh` exercises each predicate against
synthetic EXPRESSION nodes whose `.exp.op` is one of `. -> [] () +`.
