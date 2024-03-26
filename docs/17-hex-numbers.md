# ch17 - implementing hexadecimal numbers

`0xAB75` shouldn't lex as NUMBER(0) + IDENTIFIER(`xAB75`). The trick the
book uses: add `case 'x'` to the dispatch switch. By the time we see
`x` standing alone (i.e. the preceding char was a digit that already
became a NUMBER token), we pop that NUMBER off the vector and replace
the pair with one NUMBER carrying the hex value.

New helpers in `lexer.c`:

- `lexer_pop_token` - `vector_pop` on the token vector. Used by
  `token_make_special_number` to discard the leading `0`.
- `is_hex_char` / `read_hex_number_str` - the digit-eater.
- `token_make_special_number_hexadecimal` - consume `x`, eat hex
  digits, `strtol(s, 0, 16)`, emit NUMBER.
- `token_make_special_number` - the dispatch entry. Pops the previous
  NUMBER, peeks: if `x`, route to the hex path. Ch18 widens this to
  cover `0b` for binary.

This is a small abuse of the per-byte dispatch model (we let the lexer
peek backward by popping the previous token), but it's how the book
threads the needle without rewriting the read loop.

Smoke test (`tests/14-hex-numbers.sh`) feeds `0xAB75 0xff 0x10` and
asserts three NUMBER tokens with the expected decimal values.
