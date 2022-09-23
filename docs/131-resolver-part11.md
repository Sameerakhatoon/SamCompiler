# ch131 - creating the resolver - Part 11

Parenthesised expressions, unsupported nodes, and casts get follow
handlers.

What landed in `resolver.c`:
- `resolver_follow_exp_parenthesis`: just steps into
  `node->parenthesis.exp`.
- `resolver_follow_unsupported_unary_node`: steps into the operand.
- `resolver_follow_unsupported_node`: dispatches by node type (only
  UNARY for now), then always pushes an UNSUPPORTED marker entity.
- `resolver_follow_cast`: walks the operand via
  `_follow_unsupported_node`, sets `WAS_CASTED` on the resulting top
  entity, and pushes a CAST entity carrying the target dtype.
- `resolver_follow_part_return_entity`: extended with
  EXPRESSION_PARENTHESES and CAST cases.

Test: `tests/79-resolver-cast-paren.sh` builds a synthetic
`CAST(int) NUMBER(99)` node, runs `resolver_follow_cast` directly,
and confirms the result has two entities with a CAST on top.
