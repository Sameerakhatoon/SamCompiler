# ch44 - implementing variables (part 3)

Array declarators: `int x[4][3];` works.

New file `array.c` with:
- `array_brackets_new` / `_free` - allocate / free the
  `struct array_brackets { struct vector* n_brackets; }`.
- `array_brackets_add(brackets, node)` - push a NODE_TYPE_BRACKET
  into the vector.
- `array_brackets_node_vector` - accessor.
- `array_brackets_calculate_size{,_from_index}` - placeholder
  returning 0 for now; real sizing arrives later.
- `array_total_indexes(dtype)` - returns the bracket count for an
  array datatype.

`compiler.h`:
- forward decl `struct array_brackets` so `struct datatype` can hold
  a pointer.
- `struct datatype` gains `array { brackets, size }`.
- `struct node` composite union gains `struct bracket { inner }`.
- `make_bracket_node(inner)` and `array_brackets_*` prototypes.

`node.c`:
- `make_bracket_node(inner)` - emit a NODE_TYPE_BRACKET with one
  child expression.

`parser.c`:
- `expect_op(op)` helper (mirror of `expect_sym`).
- `parse_array_brackets(history)` - loop: `[` then either `]` (empty)
  or `expression ]`. Each bracket becomes a NODE_TYPE_BRACKET node,
  collected into an `array_brackets`.
- `parse_variable` checks for `[` before `=`: if present, call
  `parse_array_brackets`, attach to the datatype's `.array.brackets`,
  compute `.array.size`, set `DATATYPE_FLAG_IS_ARRAY`.

Smoke test (`tests/37-array-decl.sh`) feeds `int x[4][3];` and
asserts the variable's datatype has `IS_ARRAY` set, with two bracket
nodes whose inner values are 4 and 3.
