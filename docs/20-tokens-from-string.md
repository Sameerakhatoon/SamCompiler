# ch20 - creating tokens outside of the input file

This is why the lexer talked to its input through a v-table back in
ch7: today we swap that v-table out to read from a string instead of
a FILE*.

New helpers in `lexer.c`:

- `lexer_string_buffer_next_char` / `peek_char` / `push_char` - thin
  wrappers around `buffer_read` / `buffer_peek` / `buffer_write` on a
  buffer stashed in `lex_process->private`.
- `lexer_string_buffer_functions` - the v-table that wires them up.
- `tokens_build_for_string(compiler, str)` - convenience: allocate a
  buffer, copy the string into it, create a lex_process with the
  string v-table and the buffer in `private`, run `lex`, return the
  populated lex_process.

Why bother now? The preprocessor (Module 4) needs to re-lex macro
expansions: `#define FOO 1+1` then `int x = FOO * 2;` requires the
preprocessor to tokenize `1+1` independently of the surrounding file
stream. This chapter installs the plumbing.

Smoke test (`tests/17-tokens-from-string.sh`) calls
`tokens_build_for_string(cp, "int x = 42")` and asserts the resulting
token vector has KW int, ID x, OP =, NUM 42.
