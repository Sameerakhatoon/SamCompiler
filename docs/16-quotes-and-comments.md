# ch16 - handling quotes (and comments) in the lexer

Three related additions, all flowing through the lexer dispatch:

## Comments

`handle_comment` runs at the top of `read_next_token` before the
switch. If the peeked char is `/`, it eats it, then peeks again:
- `/` -> `token_make_one_line_comment` - eat to newline/EOF, emit
  `TOKEN_TYPE_COMMENT`.
- `*` -> `token_make_multiline_comment` - read until `*/`, fail if EOF
  hits first.
- otherwise push `/` back and let `token_make_operator_or_string`
  handle it as the division operator.

This is why `/` is the one operator char excluded from
`OPERATOR_CASE_EXCLUDING_DIVISION` - comments hijack it first.

## Char literals

`token_make_quote` handles `'X'` (the apostrophe lands in its own
`case '\''` arm). One char (or `\<esc>`) between the quotes;
`lex_get_escaped_char` resolves `\n`, `\\`, `\t`, `\'`. The result is
emitted as `TOKEN_TYPE_NUMBER` (cval = the byte), not a separate
"char" type, because in C a char literal is just its integer value.

## assert_next_char

Tiny new helper: `nextc` + `assert(c == expected)`. Used to consume a
char we already peeked at (the opening `'` in `token_make_quote`).

Smoke test (`tests/13-quotes-and-comments.sh`) feeds a file with a
line comment, a block comment, and `'A' '\n' '\t'`, then asserts the
counts and that `'A'`, `'\n'`, `'\t'` decode to 65, 10, 9.
