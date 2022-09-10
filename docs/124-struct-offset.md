# ch124 - implementing the struct_offset function

Replaces the ch122 stub with the real implementation.

What landed in `node.c`:
- `node_is_struct_or_union(node)`: true iff node is STRUCT or UNION.

What landed in `helper.c`:
- `body_largest_variable_node(body)`: returns `body->body.largest_var_node`,
  NULL-safe.
- `variable_struct_or_union_largest_variable_node(var)`: shortcut
  through `variable_struct_or_union_body_node`.
- `struct_offset(compiler, struct_name, var_name, *var_node_out,
   last_pos, flags)`:
  - Looks the struct up via `symresolver_get_symbol`. Asserts it's
    STRUCT-or-UNION.
  - Iterates the struct body's statements vector (forward or
    backward via `STRUCT_ACCESS_BACKWARDS`), summing each previous
    member's size and aligning to the current member.
  - Primitive alignment uses the current member's size; struct /
    union alignment uses its largest variable node's size.
  - Stops as soon as the current member's name matches `var_name`,
    leaving `*var_node_out` pointing at it.
  - Unsets `VECTOR_FLAG_PEEK_DECREMENT` on exit so the caller's
    vector iteration state isn't sticky.

What landed in `compiler.h`:
- `STRUCT_ACCESS_BACKWARDS` / `STRUCT_STOP_AT_POINTER_ACCESS` enum.
- Forward decls for the three new helpers.

Test: `tests/71-struct-offset.sh` parses
`struct s { int a; char b; int c; }` and checks offsets `a=0`,
`b=4`, `c=8` (the `char` pushes position to 5, then the next `int`
aligns up to 8).
