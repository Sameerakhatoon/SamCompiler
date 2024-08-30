# ch135 - creating the resolver - Part 15

`resolver_finalize_result` (and friends) get bodies, closing out the
resolver scaffolding.

What landed in `datatype.c`:
- `datatype_is_struct_or_union_non_pointer(dtype)`: true iff dtype
  is a struct/union value (not a pointer to one) with a known
  primary type.

What landed in `resolver.c`:
- `resolver_finalize_result_flags(resolver, result)`: walks the
  entity chain and ORs in result flags for codegen:
  `FIRST_ENTITY_PUSH_VALUE` vs `FIRST_ENTITY_LOAD_TO_EBX`,
  `FINAL_INDIRECTION_REQUIRED_FOR_VALUE`, `DOES_GET_ADDRESS`. Single
  struct-value -> LOAD_TO_EBX; DO_INDIRECTION -> LOAD_TO_EBX +
  FINAL_INDIRECTION; UNARY_GET_ADDRESS -> DOES_GET_ADDRESS;
  FUNCTION_CALL -> LOAD_TO_EBX; ARRAY_BRACKET branches by pointer
  flag; GENERAL -> LOAD_TO_EBX + FINAL_INDIRECTION. Tail rule for
  trailing arrays drops the final indirection in the no-bracket
  case.
- `resolver_finalize_unary(resolver, result, entity)`: inherit
  scope/dtype/offset from prev, then for `*` reduce pointer_depth
  (drop IS_POINTER at 0) and for `&` bump it.
- `resolver_finalize_last_entity(resolver, result)`: dispatch to
  `_finalize_unary` when the tail is a unary entity.
- `resolver_finalize_result(resolver, result)`: invoke the user's
  `set_result_base` callback on the first entity, then run the
  two helpers above.

Book operator-precedence quirk preserved in
`resolver_finalize_result_flags`:
`flags &= ~FLAG_A | FLAG_B` parses as `flags &= ~(FLAG_A | FLAG_B)`
because `|` binds looser than `&`. We replicate verbatim.

Test: `tests/83-resolver-finalize.sh` confirms `set_result_base`
fires once for a simple int variable follow, the result keeps
`FIRST_ENTITY_PUSH_VALUE`, and a synthetic struct-value entity
flips to `FIRST_ENTITY_LOAD_TO_EBX`.
