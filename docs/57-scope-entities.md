# ch57 - implementing parser scope entities and functionalities

Small chapter: introduces `struct parser_scope_entity` so the parser
has somewhere to write per-variable stack offsets and flags as
declarations come in.

What landed in `parser.c`:

- `PARSER_SCOPE_ENTITY_ON_STACK` / `_STRUCTURE_SCOPE` flag enum.
- `struct parser_scope_entity { flags, stack_offset, node }`.
- `parser_new_scope_entity(node, stack_offset, flags)` - calloc +
  fill.
- `parser_scope_push(node, size)` - thin wrapper over
  `scope_push(current_process, node, size)`.

No behaviour change yet. ch58-60 hook these up: the variable parser
will call `parser_new_scope_entity` per declaration and
`parser_scope_push` to keep the scope chain in sync.
