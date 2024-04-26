# ch34 - implementing datatypes and keywords (part 2)

Fills out `parse_datatype_type` from ch33 with the real machinery.

New file `datatype.c` (just `datatype_is_struct_or_union_for_name` for
now; later chapters add sizing helpers).

`token.c` grows:
- `token_is_operator(t, val)`.
- `token_is_primitive_keyword(t)` using a private `primitive_types[]`
  table (void/char/short/int/long/float/double).

`compiler.h` gets the `DATA_TYPE_EXPECT_*` enum (PRIMITIVE / UNION /
STRUCT) so `parse_datatype_type` can record what kind of declarator
it's looking at.

`parser.c` additions:
- `token_next_is_operator(op)` - convenience peek.
- `parser_get_datatype_tokens(&dt, &dt2)` - consume the datatype
  keyword + optional secondary primitive (`long int` -> dt=long,
  dt2=int).
- `parser_datatype_expected_for_type_string(str)` - returns
  DATA_TYPE_EXPECT_*.
- `parser_get_random_type_index` / `parser_build_random_type_name` -
  forge `customtypename_NN` identifiers for anonymous struct/union.
- `parser_get_pointer_depth` - count leading `*` operators.
- `parser_datatype_init` / `parser_datatype_init_type_and_size` -
  empty stubs the next chapters fill in.
- `parse_datatype_type` rewritten: gets the datatype tokens, handles
  struct/union (with synthesized name when anonymous), counts pointer
  depth, sets `IS_POINTER` when nonzero, stamps `type_str` and
  `pointer_depth`.

Smoke test (`tests/29-datatype-pointer.sh`) feeds `int**` and confirms
the parser handles it without crashing.
