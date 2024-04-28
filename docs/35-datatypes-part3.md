# ch35 - implementing datatypes and keywords (part 3)

`parser_datatype_init` and `parser_datatype_init_type_and_size` get
real implementations.

What landed in `parser.c`:

- `parser_datatype_is_secondary_allowed(expected_type)` - secondaries
  only with PRIMITIVE (not struct/union).
- `parser_datatype_is_secondary_allowed_for_type(type)` - secondaries
  only with long / short / double / float.
- `parser_datatype_init_type_and_size_for_primitive(...)` - the big
  switch: maps "int" -> DATA_TYPE_INTEGER / DATA_SIZE_DWORD, "char" ->
  CHAR/BYTE, etc.
- `parser_datatype_adjust_size_for_secondary(...)` - for `long int`,
  builds a secondary datatype and pins it on `.secondary`, sums the
  sizes, sets DATATYPE_FLAG_IS_SECONDARY.
- `parser_datatype_init_type_and_size(...)` switches on expected_type;
  struct/union currently `compiler_error` "unsupported".
- `parser_datatype_init` wires `init_type_and_size` + stamps
  `type_str`. Warns and clamps `long long` to 32-bit.

`compiler.h` gets the `DATA_SIZE_*` enum (ZERO/BYTE/WORD/DWORD/DDWORD).

**Heads up:** the `double` arm in
`parser_datatype_init_type_and_size_for_primitive` ships a typo from
the book - writes `DATA_TYPE_DOUBLE` into `.size` (then overwrites
with `DATA_SIZE_DWORD`) so `.type` for double stays 0 (VOID). Caught
in **g02**.

Smoke test (`tests/30-datatype-primitives.sh`) feeds each primitive
type spelling through `compile_file` and asserts they all parse OK.
