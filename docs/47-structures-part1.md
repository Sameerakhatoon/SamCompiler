# ch47 - implementing structures (part 1)

The parser now recognises `struct foo { ... };` declarations -
**recognises**, not yet fully parses. The body is walked-and-discarded
by a stub; ch48+ fills in real body parsing.

What landed:

- `datatype.c::datatype_is_struct_or_union(dtype)` - true if
  `dtype->type` is DATA_TYPE_STRUCT or DATA_TYPE_UNION.
- `parser_datatype_init_type_and_size`'s struct/union arms no longer
  `compiler_error`; they stamp `.type` to STRUCT/UNION and size 0.
  Real sizing comes later.
- `parse_struct_no_new_scope(dtype)` - **stub** that swallows the
  brace block, including any nested braces.
- `parse_struct(dtype)` - opens a new scope (unless the input is a
  forward declaration with no `{`), calls the body parser, closes the
  scope.
- `parse_struct_or_union(dtype)` - dispatches on `dtype->type`. Union
  arm is empty (ch48+).
- `parse_variable_function_or_struct_union` checks
  `datatype_is_struct_or_union + next is '{'` and routes there. If
  the input is bare `struct abc {};` (no declarator after), we eat the
  semicolon and bail.
- `parse_scope_new` / `_finish` wrap `scope_new` / `scope_finish` so
  the parser can call them without threading `current_process` around.
- `parse()` now calls `scope_create_root(process)` so the scope chain
  exists from the start.

Smoke test (`tests/39-struct-empty.sh`) feeds `struct abc { };` and
asserts `compile_file` returns OK.
