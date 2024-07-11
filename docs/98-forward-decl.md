# ch98 - parsing forward declarations

`struct dog;` is now a valid top-level form.

What landed in `parser.c`:
- `parse_forward_declaration(dtype)`: thin wrapper around
  `parse_struct(dtype)`. `parse_struct` already detects a missing `{`
  body and skips the scope dance, and `make_struct_node` sets
  `NODE_FLAG_IS_FORWARD_DECLARATION` when the body pointer is NULL.
- `parse_variable_function_or_struct_union`: after `parse_datatype`,
  if the next token is `;`, route to `parse_forward_declaration`
  instead of expecting a variable name.

Test: `tests/54-forward-struct-decl.sh` parses
`struct dog; struct dog { int x; };`, confirms two top-level nodes,
the first with NULL body and the FORWARD_DECLARATION flag set, the
second with a real body.
