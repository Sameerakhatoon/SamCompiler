# ch73 - parsing function arguments

`int add(int a, int b) { }` now parses with both parameters captured
into `func.args.vector`.

What landed in `parser.c`:

- `token_read_dots(amount)` - consume exactly N `.` operators (for
  `...`).
- `parse_variable_full(history)` - parse a datatype + optional
  identifier and route through `parse_variable`. Used for parameter
  parsing where the name is optional in prototypes.
- `parse_function_arguments(history)`:
  1. Open a parser scope.
  2. While not `)`:
     - if `.`, consume `...` (variadic) and break.
     - `parse_variable_full` with `HISTORY_FLAG_IS_UPWARD_STACK` set.
     - Pop the resulting node, push into arguments_vec.
     - If next is `,`, eat it; else break.
  3. Close the scope, return the vector.
- `parse_function` calls `parse_function_arguments` between
  `expect_op("(")` and `expect_sym(')')`.
- `parser_scope_offset_for_stack` widened: when
  `HISTORY_FLAG_IS_UPWARD_STACK` is set, offsets are positive and grow
  upward (the previous arg's offset + its size).

Smoke test (`tests/46-function-args.sh`) feeds
`int add(int a, int b) { }` and asserts the function has two args
named "a" and "b".
