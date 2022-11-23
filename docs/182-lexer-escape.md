# ch182 - escaping characters

`"a\tb"` and friends now produce real escape sequences in the
lexer instead of silently dropping the backslash.

What landed in `lexer.c`:
- `lex_handle_escape_number(buf)`: reads a numeric escape (`\123`)
  and writes the byte (0-255).
- `lex_handle_escape(buf)`: dispatches by the next char - digit -
  numeric path; otherwise look up via `lex_get_escaped_char`
  (already present from ch9) and write the resulting byte.
- `token_make_string` now calls `lex_handle_escape(buf)` instead
  of just `continue`ing past the backslash.

Test: `tests/120-lexer-escape.sh` lexes / parses
`char* s = "a\tb";` and confirms the resulting string is exactly
3 bytes long with chars `a` (97), TAB (9), `b` (98).
