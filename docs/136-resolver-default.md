# ch136 - creating the resolver default handler

New `rdefault.c` ships the standard implementation of every
`resolver_callbacks` slot, so codegen can call into the resolver
without supplying its own merge / private / address-build logic.
Also corrects the ch122 typo `resolver_regster_function` to
`resolver_register_function` everywhere it appears.

What landed in `compiler.h`:
- `RESOLVER_DEFAULT_ENTITY_TYPE_*` (STACK / SYMBOL).
- `RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK`.
- `RESOLVER_DEFAULT_ENTITY_DATA_TYPE_*` (VARIABLE / FUNCTION /
  ARRAY_BRACKET).
- `struct resolver_default_entity_data { type, address[60],
   base_address[60], offset, flags }`.
- `struct resolver_default_scope_data { flags }`.
- Public surface forward decls (private accessors, address helpers,
  make_private / set_result_base / merge_entities,
  new_array_entity, delete_*, new_scope_entity, register_function,
  new_scope / finish_scope, new_process).
- `resolver_regster_function` -> `resolver_register_function`.

What landed in `rdefault.c`:
- `resolver_default_stack_asm_address(stack_offset, out)`:
  formats `ebp-4` / `ebp+4`.
- `resolver_default_global_asm_address(name, offset, out)`:
  formats `var` / `var+4`.
- `resolver_default_entity_data_set_address`: chooses between the
  stack-offset and global-name flavours by IS_LOCAL_STACK.
- `resolver_default_make_private(entity, node, offset, scope)`:
  allocates entity_data, propagates IS_STACK -> IS_LOCAL_STACK,
  stamps offset/flags/type, and resolves the address.
- `resolver_default_set_result_base`: copies private entity_data's
  address / base_address / offset onto `result->base`.
- `resolver_default_new_entity_data_for_var_node` /
  `_for_array_bracket` / `_for_function`: typed entity-data factories.
- `resolver_default_new_scope_entity`: var entity + entity_data
  bundle; calls `resolver_new_entity_for_var_node` so the result
  ends up on the current scope's `entities` vector.
- `resolver_default_register_function`: function entity +
  entity_data bundle.
- `resolver_default_new_scope` / `_finish_scope`: scope_data alloc
  pair.
- `resolver_default_new_array_entity`: returns entity_data tagged
  ARRAY_BRACKET.
- `resolver_default_delete_entity` / `_delete_scope`: free
  `entity->private` / `scope->private`.
- `resolver_default_merge_array_calculate_out_offset`: helper used
  by the merge code path.
- `resolver_default_merge_entities(L, R)`: synthesizes a new entity
  inheriting L's flags / scope / node, R's type / dtype / array,
  and `L->offset + R->offset`.
- `resolver_default_new_process(compiler)`: bundles all six default
  callbacks and constructs a resolver_process.

What landed in `Makefile`: new `rdefault.o` target added to
`OBJECTS`.

Test: `tests/84-resolver-default.sh` parses `int v;`, registers the
variable through `resolver_default_new_scope_entity`, runs
`resolver_follow` on an IDENTIFIER node, and confirms
`result->base.address`, `base_address`, and `offset` reflect the
global-variable address format.
