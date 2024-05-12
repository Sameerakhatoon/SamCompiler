# ch38 - implementing the scopes

ch37 is a lecture-only chapter (no source attachment). ch38 introduces
`scope.c` and the `struct scope` machinery.

What landed:

- `struct scope` (compiler.h) - flags + entity vector + size + parent.
- `compile_process` gets a `scope { root, current }` block.
- `scope.c` API:
  - `scope_create_root(process)` / `scope_free_root` - bootstrap and
    tear down.
  - `scope_new(process, flags)` / `scope_finish(process)` - push and
    pop nested scopes.
  - `scope_push(process, ptr, elem_size)` - push an entity pointer
    into the current scope and bump the byte tally.
  - `scope_last_entity`, `scope_last_entity_stop_at`,
    `scope_last_entity_from_scope_stop_at`,
    `scope_last_entity_at_scope` - walk back through the entity
    vectors, optionally up parents, stopping at a given scope.
  - `scope_iteration_start` / `scope_iterate_back` /
    `scope_iteration_end` - bidirectional iteration helpers.
  - `scope_current(process)` - convenience.

The scope vectors start with PEEK_DECREMENT set so the most-recent
push is what `vector_peek_ptr` returns first - matches how name
resolution walks backwards through declarations.

Smoke test (`tests/32-scopes.sh`) creates a root scope, pushes one
entity, opens a child scope, pushes another, asserts last-entity
returns the child's, then closes the child and asserts it returns the
root's again.
