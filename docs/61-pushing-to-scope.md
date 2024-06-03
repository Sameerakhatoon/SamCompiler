# ch61 - pushing variables to the scope

The variable parser now actually registers what it builds. Each new
declaration gets a `parser_scope_entity` (with `node + flags +
stack_offset`) and is `scope_push`'d into the current scope.

What landed in `parser.c`:

- `parser_scope_push(entity, size)` retyped to take a
  `struct parser_scope_entity*` (was a raw `struct node*`).
- `parser_scope_last_entity()` thin wrapper over
  `scope_last_entity(current_process)`.
- `make_variable_node_and_register` finishes the loop: after
  computing `var.aoffset` via `parser_scope_offset`, build a fresh
  entity wrapping the var node and push it.

Now `parser_scope_offset_for_stack` and `_for_structure` actually
find chains of prior declarations when computing offsets - the math
they do has been correct since ch58/60, but the scope was empty
before this chapter.

No new dedicated test - existing variable / variable-list /
array-decl tests still pass; codegen tests in later chapters will
exercise the offsets in earnest.
