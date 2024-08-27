# ch132 - creating the resolver - Part 12

Real unary handling + a default unsupported fallback.

What landed in `compiler.h`:
- `resolver_entity.process` renamed to `.resolver` (less shadow).
- Forward decl for `op_is_address`.

What landed in `helper.c`:
- `op_is_address(op)`: `&`.

What landed in `resolver.c`:
- `resolver_follow_indirection(resolver, node, result)`: walk the
  operand, push a UNARY_INDIRECTION entity carrying the recorded
  depth.
- `resolver_follow_unary_address(resolver, node, result)`: walk the
  operand, push a UNARY_GET_ADDRESS entity inheriting its dtype /
  scope / offset.
- `resolver_follow_unary(resolver, node, result)`: dispatch by
  `op_is_indirection` / `op_is_address`.
- `resolver_follow_part_return_entity` gets the UNARY case + a
  `default` clause that drops into `_follow_unsupported_node`. On
  exit it stamps `entity->result = result; entity->resolver = resolver`.

Test: `tests/80-resolver-unary.sh` registers `int v;`, builds a
synthetic `*v` UNARY at depth 1, runs `resolver_follow`, and
confirms the top entity is `UNARY_INDIRECTION` and that result and
resolver are stamped on it.
