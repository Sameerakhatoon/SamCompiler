# ch94 - case indexes / register case function

Backfills the switch-case registration infrastructure (which ch85/ch89
skipped here) and wires the case index.

What landed in `parser.c`:
- `HISTORY_FLAG_IN_SWITCH_STATEMENT` added.
- `struct parsed_switch_case { int index; }`,
  `struct history_cases { vector* cases; bool has_default_case; }`, and
  `struct parser_history_switch { history_cases case_data; }` declared.
- `struct history` gains `_switch`.
- `parser_new_switch_statement` zero-inits the `_switch`, creates the
  cases vector, sets the flag. `parser_end_switch_statement` is a stub.
- `parser_register_case` asserts the flag and pushes a
  `parsed_switch_case { .index = case_node->stmt._case.exp->llnum }`.
- `parse_switch` now opens with `parser_new_switch_statement` and feeds
  its `case_data.cases` to `make_switch_node`.
- `parse_case` rejects non-NUMBER case expressions and registers the
  case via `parser_register_case`.

Known carry-over bug from the book (see Gxx): `parse_case` pops the
case node off the parser's node stack to hand to
`parser_register_case`, but never pushes it back. The first `case` in
a switch body therefore causes `parse_body_multiple_statements` to pop
an empty node stack and abort. We replicate the book verbatim here and
fix it in a separate gotcha commit with a test.
