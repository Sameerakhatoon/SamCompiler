# ch6 - creating our token structures

Added the data the lexer is going to fill in:

- `struct pos` carries `line/col/filename` so we can attach a source
  location to every token (and later, every AST node).
- The `TOKEN_TYPE_*` enum names the eight kinds of token the lexer will
  emit: identifier, keyword, operator, symbol, number, string, comment,
  newline.
- `struct token` itself - the result of one lexer step. The value is in
  an anonymous union (`cval` / `sval` / `inum` / `lnum` / `llnum` /
  `any`) chosen by `type`. Two extra fields: `whitespace` records
  whether whitespace separates this token from the next one (matters
  for things like `*a` vs `* a`), and `between_brackets` remembers the
  original parenthesised text for later pretty-printing.

No lexer logic yet - that arrives in ch7. This commit is only the
shapes that ch7 will write into.
