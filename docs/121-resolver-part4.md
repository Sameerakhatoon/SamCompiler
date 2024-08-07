# ch121 - creating the resolver - Part 4

More entity factories. No driver pass yet; each function here is
called by later chapters' entity-resolution loop.

What landed in `resolver.c` (and matching `compiler.h` decls):
- `resolver_create_new_entity_for_merged_array_bracket(...)`: like
  the ch119 version but does NOT precompute an offset (the merge
  pass folds it in later).
- `resolver_create_new_unknown_entity(...)`: a `GENERAL` entity for
  resolver dead-ends we still want to record (offset / scope / dtype
  known, identity unknown). Both NO_MERGE flags set.
- `resolver_create_new_unary_indirection_entity(...)`: `*p`, `**p`,
  with the indirection depth recorded.
- `resolver_create_new_unary_get_address_entity(...)`: `&a.b.c`.
  Bumps the entity's dtype one pointer level deeper.
- `resolver_create_new_cast_entity(...)`: `(T) expr`. Records the
  target dtype and scope; no bound node.
- `resolver_create_new_entity_for_var_node_custom_scope(...)` plus
  the current-scope wrapper `_for_var_node`.
- `resolver_new_entity_for_var_node_no_push(...)` and
  `resolver_new_entity_for_var_node(...)` (the latter also pushes
  onto the current scope's `entities` vector). Stack-scope variables
  get `RESOLVER_ENTITY_FLAG_IS_STACK`.

Book quirk preserved: the var-node factories pass `NODE_TYPE_VARIABLE`
as the entity-type argument to `resolver_create_new_entity`, which is
the parser enum, not the resolver enum. We replicate verbatim.

Test: `tests/69-resolver-entity-factories.sh` builds each factory
result and asserts its `type` / `flags` / `dtype` shape.
