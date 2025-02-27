# ch244 - implementing validator scopes

Plumbs validator-owned scope state + tree iteration onto the
ch243 skeleton. The validator now borrows the default resolver's
scope manager so symbol lookups during validation agree with
what codegen will see.

What landed in `validator.c`:
- Module-private state: `validator_current_compile_process`,
  `current_function` (unused for now; later chapters thread
  this).
- `validation_new_scope(flags)`: delegates to
  `resolver_default_new_scope` on the active compile_process.
- `validation_end_scope()`: delegates to
  `resolver_default_finish_scope`.
- `validation_next_tree_node()`: peek_ptrs the next entry off
  `process->node_tree_vec`.
- `validate_initialize(process)`: stashes the process,
  rewinds the tree peek pointer, opens a fresh symresolver
  table via `symresolver_new_table`.
- `validate_destruct(process)`: closes that table via
  `symresolver_end_table` and rewinds the peek pointer again.

Test: `tests/171-validator-scopes.sh` confirms a trivial source
still reaches codegen end-to-end after the new
init/destruct round-trip, and that `validate()` against a fresh
compile_process still returns `VALIDATION_ALL_OK`.
