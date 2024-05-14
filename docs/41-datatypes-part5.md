# ch41 - implementing datatypes and keywords (part 5)

Small addition: tolerate the decorative `int` in `long int`,
`float int`, `double int`. The book treats the trailing `int` as
purely cosmetic - the real type stays long/float/double.

Two helpers in `parser.c`:

- `parser_is_int_valid_after_datatype(dtype)` - returns true for
  LONG / FLOAT / DOUBLE.
- `parser_ignore_int(dtype)` - if the next token is the keyword `int`,
  swallow it; if the preceding datatype doesn't allow it,
  `compiler_error`.

`parse_variable_function_or_struct_union` calls `parser_ignore_int`
right after `parse_datatype` so any trailing `int` is gone before the
rest of the declaration parsing.

(The book has duplicate logic with ch35's `long long` warning, but
that lives in `parser_datatype_init`; this layer handles the
declarator-level abbreviation. Both stay.)

Smoke test (`tests/34-int-abbreviation.sh`) compiles `long int` and
`double int` and asserts both succeed.
