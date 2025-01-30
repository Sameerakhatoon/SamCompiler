# ch233 - fixing some issues with casting pointers

Tightens the resolver's handling of pointer-cast member access
patterns like `&((struct T*)0)->y` (used everywhere by
`offsetof`-style macros).

What landed in `resolver.c`:
- `resolver_do_indirection(entity)`: helper - true unless the
  entity is a FUNCTION_CALL result, the surrounding result
  already wants the address
  (RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS), or the entity is a
  CAST. Previously the `->` branch of
  `resolver_follow_struct_exp` only filtered out function-call
  results; the new helper also keeps unary-& and cast results
  from being indirected through.
- `resolver_follow_struct_exp`'s `->` branch now calls
  `resolver_do_indirection(left_entity)` instead of the
  narrow function-call check.
- `resolver_follow_cast`: when the cast target is a struct or
  union, set `cast_entity->scope` to the current scope if
  unset and anchor `result->last_struct_union_entity` to the
  cast so subsequent `.` / `->` can resolve member offsets
  against the cast dtype.
- `resolver_follow_unary_address`: set
  `RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS` on the result
  before walking the operand, so any inner `->` skips
  indirection (per the helper above).

Test: `tests/163-resolver-cast-pointer.sh` compiles `return
&((struct dog*)0)->y;` against a struct that puts `y` at
offset 4, confirms main reaches codegen successfully, and
that the emitted output references the offset.
