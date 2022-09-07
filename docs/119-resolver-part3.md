# ch119 - creating the resolver - Part 3

Array-bracket helpers and two more resolver entity factories.

What landed in `compiler.h`:
- `resolver_array.multiplier` dropped (recomputed on demand from the
  bracket vector via `array_multiplier`).
- Forward decls for `array_multiplier`, `array_offset`,
  `resolver_create_new_entity_for_unsupported_node`, and
  `resolver_create_new_entity_for_array_bracket`.

What landed in `helper.c`:
- `array_multiplier(dtype, index, index_value)`: walks the bracket
  vector starting at `index + 1` and multiplies each declared
  dimension into `index_value`. Returns `index_value` unchanged for a
  non-array datatype.
- `array_offset(dtype, index, index_value)`: for the last bracket
  (or a non-array), returns `index_value * element_size`; otherwise
  multiplies by `array_multiplier(...)`.

What landed in `resolver.c`:
- `resolver_create_new_entity_for_unsupported_node(result, node)`:
  wraps an unsupported AST node in an entity flagged
  `NO_MERGE_WITH_LEFT_ENTITY | NO_MERGE_WITH_NEXT_ENTITY`.
- `resolver_create_new_entity_for_array_bracket(result, process,
   node, idx_node, index, dtype, private, scope)`: records the
  bracket index, the (possibly runtime) index expression, the
  datatype, and the pre-computed byte offset via `array_offset`.
  Non-NUMBER index nodes default to `1` so the offset comes back to
  one element-size; the runtime path will later multiply by the real
  index.

Test: `tests/68-array-offset.sh` parses `int x[4][3];` and verifies
`array_offset` for index 0 (outer bracket: 0, 12, 24 bytes for
i = 0, 1, 2) and index 1 (last bracket: 8 bytes for j = 2). Element
size 4 matches the int.
