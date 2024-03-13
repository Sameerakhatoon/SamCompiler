# ch8 - creating a number token

First real lexer output. Reading "5837 2837 3827 1028 4937" yields five
`TOKEN_TYPE_NUMBER` tokens, each carrying its decimal value in the
union's `llnum` slot.

What landed:

- `NUMERIC_CASE` macro in `compiler.h` - one shared switch label for the
  ten ASCII digits, used by the lexer dispatch.
- `compiler_error` / `compiler_warning` in `compiler.c` - both print the
  formatted message, then a `" on line N, col M in file F"` suffix.
  `compiler_error` calls `exit(-1)`.
- `struct token` gained a `struct pos pos` field so every token carries
  its source location.
- `cprocess_create` now stashes `filename` into `cfile.abs_path` and
  initialises `compile_process.pos` to (1, 1) so error messages have
  something to print before the lexer starts.
- `lexer.c` gained the read-loop machinery:
  - `peekc` / `nextc` / `pushc` - thin wrappers over the v-table that
    keep `lex_process.pos` (line/col) in sync.
  - `token_create` - copy the caller's struct into a static
    `tmp_token`, stamp it with the current pos, return its address.
  - `read_number_str` / `read_number` - use the `LEX_GETC_IF` macro to
    accumulate consecutive digits into a buffer, then `atoll`.
  - `handle_whitespace` - bumps the previous token's `whitespace=true`
    flag and recurses.
  - `read_next_token` switch: digits -> number, space/tab ->
    whitespace, EOF -> NULL, anything else -> `compiler_error`.
- `lex()` is now real: sets `pos.filename`, then pumps `read_next_token`
  into `process->token_vec` until EOF.
- `test.c` is now "5837 2837 3827 1028 4937" so `./main` exercises the
  number path.

Smoke test (`tests/06-number-tokens.sh`) builds a probe that drives the
lexer on `test.c` and asserts five tokens, correct values, correct
type, and that whitespace flags are set.
