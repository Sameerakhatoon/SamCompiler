# ch33 - implementing datatypes and keywords (part 1)

The parser starts handling declarations. New machinery:

- `enum DATATYPE_FLAG_*` (11 flags) - signed/static/const/pointer/array
  /extern/restrict/ignore-typecheck/secondary/no-name/literal.
- `enum DATA_TYPE_*` - void / char / short / int / long / float /
  double / struct / union / unknown.
- `struct datatype` - flags + type + optional secondary (for
  "long int") + spelling + size + pointer_depth + struct/union node.
- `keyword_is_datatype(str)` - subset of `is_keyword` that names types.
  Lives in `lexer.c`.
- `parser.c::is_keyword_variable_modifier` - the prefix-class keywords
  (signed / unsigned / static / const / extern / __ignore_typecheck__).
- `parse_datatype_modifiers(dtype)` - consume the modifier run,
  OR-ing flags.
- `parse_datatype_type(dtype)` - **stub here** that consumes one
  datatype keyword and coarsely maps to DATA_TYPE_*. ch34 replaces this
  with the proper long/short/unsigned handling.
- `parse_datatype(dtype)` - `modifier* type modifier*`. Defaults
  `IS_SIGNED` on.
- `parse_variable_function_or_struct_union(history)` - ch33 just
  parses + discards the datatype. ch34+ decides variable vs function
  vs struct.
- `parse_keyword(history)` - routes a `TOKEN_TYPE_KEYWORD` to the
  variable/function/struct path if it's a modifier or datatype.

**Heads-up:** upstream PeachCompiler ships ch33 with
`parse_datatype_type` only declared, not defined - the build doesn't
link. We ship a coarse stub here so tests stay green; ch34 widens it.

Smoke test (`tests/28-datatype-parse.sh`) confirms `compile_file` on
input `int` returns OK (datatype consumed cleanly, no crash).
