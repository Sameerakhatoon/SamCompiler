# ch22 - finalizing the lexer

Tiny but important: hand the lex_process's token vector up to the
compile_process so the next stage (parser, ch24+) can find it without
keeping a lex_process reference around.

Changes:

- `compile_process` gained a `struct vector* token_vec` field.
- `compile_file` now writes
  `process->token_vec = lex_process->token_vec;` immediately after
  `lex()` succeeds.

The lexer is now "done" for purposes of module 1: it produces a vector
of tokens, the compile_process owns it, the parser will read from it.

Smoke test (`tests/19-finalize-lexer.sh`) drives the lex pipeline,
then asserts `compile_process.token_vec` is non-NULL and has the
expected count.
