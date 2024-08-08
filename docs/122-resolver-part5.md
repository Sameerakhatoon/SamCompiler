# ch122 - creating the resolver - Part 5

More entity factories + the polymorphic builder `resolver_make_entity`.

What landed in `resolver.c`:
- `resolver_new_entity_for_rule(process, result, rule*)`: builds a
  RULE entity carrying the left/right rule flags and pushes onto the
  result chain.
- `resolver_make_entity(process, result, custom_dtype, node,
   guided_entity, scope)`: dispatch on `node->type`. VARIABLE nodes
  route to `_for_var_node_no_push`; everything else becomes a
  GENERAL unknown via `_unknown_entity`. The result inherits offset
  + flags from `guided_entity`, optionally overrides dtype, then
  gets stamped via `process->callbacks.make_private(...)`.
- `resolver_create_new_entity_for_function_call(result, process,
   left_operand_entity, private)`: FUNCTION_CALL entity, dtype
  copied from `left_operand_entity`, owns its own `arguments`
  vector (struct node*).
- `resolver_regster_function(process, func_node, private)` (book
  typo preserved): FUNCTION entity registered against the root
  scope (`process->scope.root->entities`).
- `resolver_get_entity_in_scope_with_entity_type(...)`: looks up
  by name+type. The body in the book is incomplete (no return); we
  replicate verbatim. The struct/union path calls ch124's
  `struct_offset`.

What landed in `helper.c`:
- Stub `struct_offset` returning 0. ch124 ships the real body; the
  stub exists so the linker doesn't break against ch122's resolver.

What landed in `compiler.h`:
- Forward decls for all of the above. `resolver_new_entity_for_rule`
  is declared AFTER `struct resolver_entity` because the nested
  `resolver_entity_rule` tag is only visible once that struct's
  definition is complete (C name-lookup rule).

Test: `tests/70-resolver-make-entity.sh` calls `resolver_make_entity`
with a NUMBER node (-> GENERAL=6) and a custom `make_private`
callback; asserts offset / flags propagate and `make_private` fires
exactly once. Also exercises `resolver_create_new_entity_for_function_call`
for FUNCTION_CALL=3 with an arguments vector.
