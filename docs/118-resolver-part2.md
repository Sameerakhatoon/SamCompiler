# ch118 - creating the resolver - Part 2

New `resolver.c` ships the lifecycle + accessor layer of the type
system from ch117. No actual name-resolution passes yet.

What landed in `compiler.h`:
- `resolver_process.process` renamed to `.compiler` so it doesn't
  shadow the local in helpers.
- Forward decls for the full ch118 API surface: result predicates
  (failed / ok / finished), entity walk (root / next / clone / get),
  result lifecycle (new / free / push / peek / peek_ignore_rule /
  pop), array vector accessor, compiler / scope accessors,
  scope lifecycle (new_scope_create / new_scope / finish_scope),
  process lifecycle (new_process), and `resolver_create_new_entity`.

What landed in `resolver.c`:
- All of the above, mirroring the book verbatim. Includes:
  - `resolver_new_scope` links new scope into a doubly-linked list
    after `current` and slides `current` forward.
  - `resolver_finish_scope` calls back into the user's
    `callbacks.delete_scope(scope)` before `free`.
  - `resolver_result_pop` keeps `first_entity_const` / `last_entity`
    / `entity` in sync and zeroes them once `count == 0`.
- One book quirk preserved (gated for a future gotcha): the
  `resolver_runtime_needed` helper clears
  `RUNTIME_NEEDED_TO_FINISH_PATH` rather than setting it. We
  replicate; whichever later chapter actually drives that flag will
  show whether the intent was clear-or-set.

Build wiring: new `resolver.o` target added to `Makefile`.

Test: `tests/67-resolver-lifecycle.sh` builds a resolver with a
minimal callbacks table, pushes a scope and pops it (asserting the
delete_scope callback runs), then allocates a result, pushes two
entities, peek/pop and frees - all the lifecycle invariants.
