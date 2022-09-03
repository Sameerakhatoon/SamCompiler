# ch117 - creating the resolver - Part 1

Header-only chapter. Lands the full resolver type system in
`compiler.h`; the `.c` side comes in ch118+.

What landed in `compiler.h`:
- `RESOLVER_ENTITY_FLAG_*` (8 bits) - per-entity flags
  (IS_STACK, NO_MERGE_*, DO_INDIRECTION, JUST_USE_OFFSET, etc.).
- `RESOLVER_ENTITY_TYPE_*` (11 values) - what an entity is:
  VARIABLE / FUNCTION / STRUCTURE / FUNCTION_CALL / ARRAY_BRACKET /
  RULE / GENERAL / UNARY_GET_ADDRESS / UNARY_INDIRECTION /
  UNSUPPORTED / CAST.
- `RESOLVER_SCOPE_FLAG_IS_STACK`.
- Forward decls for `resolver_result`, `resolver_process`,
  `resolver_scope`, `resolver_entity`.
- Function-pointer typedefs:
  `RESOLVER_NEW_ARRAY_BRACKET_ENTITY`, `RESOLVER_DELETE_SCOPE`,
  `RESOLVER_DELETE_ENTITY`, `RESOLVER_MERGE_ENTITIES`,
  `RESOLVER_MAKE_PRIVATE`, `RESOLVER_SET_RESULT_BASE`.
- `struct resolver_callbacks` carrying those callbacks.
- `struct resolver_process { scope.root/current; compile_process*;
  callbacks }`.
- `struct resolver_array_data { vector* array_entities; }`.
- `RESOLVER_RESULT_FLAG_*` (8 bits).
- `struct resolver_result` - the whole bookkeeping struct (first /
  identifier / last_struct_union / array_data / entity /
  last_entity / flags / count / base { address[60], base_address[60],
  offset }).
- `struct resolver_scope` - doubly-linked, with a private void*.
- `struct resolver_entity` - the big union of variant payloads
  (var_data, array, func_call_data, rule, indirection), plus
  last_resolve, dtype, scope, result, process, private, next/prev.

Test: `tests/66-resolver-types.sh` builds a tiny probe that touches
each top-level enum / struct to confirm the decls compile cleanly
and the base address buffers are 60 chars.
