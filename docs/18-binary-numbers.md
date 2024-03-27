# ch18 - implementing binary numbers

Mirror of ch17, with two new bits:

- `case 'b':` now joins `case 'x':` in the read_next_token dispatch.
- `token_make_special_number_binary` consumes `b`, reads digits with
  the existing `read_number_str` (only accepts 0-9), then
  `lexer_validate_binary_string` rejects anything that isn't '0' or
  '1'. `strtol(..., 2)` produces the integer value.
- `token_make_special_number` got a guard: it only fires if the
  previous token was NUMBER(0). Otherwise it falls back to
  `token_make_identifier_or_keyword` so a bare `b` or `x` in source
  code still works (e.g. `int box;`).

Smoke test (`tests/15-binary-numbers.sh`) feeds
`0b1110011 0b1 0xFF box ` and asserts NUM 115, NUM 1, NUM 255, plus
ID `box` (proving the fallback to identifier-handling still fires).
