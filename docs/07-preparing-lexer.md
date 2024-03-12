# ch7 - preparing our lexer

Wired up the lexer scaffolding without producing any tokens yet.

What landed:

- `struct lex_process_functions` - a v-table of three callbacks
  (`next_char`, `peek_char`, `push_char`) the lexer uses to read its
  input. This abstraction lets us swap the input source later (the
  preprocessor will feed the lexer from a buffer rather than a FILE*).
- `struct lex_process` - per-pass lexer state: current source position,
  a vector of accumulated tokens, the parent `compile_process`, a depth
  counter + buffer for parentheses, the v-table, and an opaque
  `private` blob.
- `lex_process_create` / `lex_process_free` / `lex_process_tokens` /
  `lex_process_private` accessors in `lex_process.c`.
- `compile_process_next_char` / `peek_char` / `push_char` in
  `cprocess.c` - the FILE*-backed implementation of the v-table. They
  also keep `compile_process.pos` (line/col) in sync as we read.
- `lexer.c::lex` - empty stub returning `LEXICAL_ANALYSIS_ALL_OK`. ch8
  starts populating it with the number-token branch.
- `compile_file` now creates the lex_process and calls `lex` before
  parsing.

Smoke test (`tests/05-lex-stub.sh`) builds a tiny probe that asks for a
fresh `lex_process` on `./test.c` and asserts its pos starts at 1/1
with zero tokens.
