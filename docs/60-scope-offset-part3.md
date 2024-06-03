# ch60 - implementing the variable node scope offset (part 3)

Struct-member offset path lands.

What landed in `parser.c`:

- New history flag `HISTORY_FLAG_INSIDE_STRUCTURE`.
- `parser_scope_offset_for_structure(node, history)` - struct members
  grow upward in memory: each new field starts at
  `last_entity->stack_offset + last_entity->size`, then we add any
  required alignment padding. The offset (with padding) is stamped
  onto `var.aoffset`.
- `parser_scope_offset` dispatch grows a third arm: STRUCTURE before
  the stack fallback.

Note: the upstream code reads `last_entity->node->var.type.size`
directly (so the entity better wrap a variable node). We do the
same; ch61 will start pushing real entities into the scope so this
finds something to chain off of.
