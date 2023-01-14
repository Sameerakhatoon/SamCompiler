# ch226 - processing concat directive in the preprocessor

Adds the `##` token-paste operator inside a macro function
body. `#define CAT(a, b) a ## b` + `CAT(foo, bar);` now expands
to a single `foobar` IDENTIFIER token by serializing both
operands and re-lexing.

What landed:
- `compiler.h`:
  - `FAIL_ERR(message)` assert-based bailout macro.
  - Decls for `tokens_join_buffer_write_token`,
    `tokens_join_vector`, `tokens_build_for_string`.
- `token.c`:
  - Includes `helpers/vector.h`, `helpers/buffer.h`, `assert.h`.
  - `tokens_join_buffer_write_token`: switch on token type;
    writes IDENTIFIER/OPERATOR/KEYWORD as their sval, STRING as
    `"sval"`, NUMBER as `%lld`, NEWLINE as `\n`, SYMBOL as
    `%c`. Bails with FAIL_ERR for anything else.
  - `tokens_join_vector(compiler, token_vec)`: walks token_vec
    pushing each token onto a buffer via the writer above, then
    re-lexes that buffer via `tokens_build_for_string`.
- `lexer.c`:
  - String-buffer lex v-table:
    `lexer_string_buffer_next_char` / `_peek_char` /
    `_push_char` all delegate to a `struct buffer*` stashed in
    `lex_process->private`.
  - `lexer_string_buffer_functions` exposes that v-table.
  - `tokens_build_for_string(compiler, str)`: creates a buffer
    holding `str`, sets up a lex_process with the string-buffer
    v-table, runs lex.
- `preprocessor/preprocessor.c`:
  - `preprocessor_handle_concat_part`: thin wrapper around
    `macro_function_push_something` for each side of `##`.
  - `preprocessor_handle_concat_finalize`: calls
    `tokens_join_vector` on the temp buffer, inserts the
    joined token sequence at position 0 of value_vec_target.
  - `preprocessor_handle_concat`: skips the two `#`s, reads
    the right operand, builds tmp_vec, pushes left + right
    through `concat_part`, then finalizes.
  - `preprocessor_is_next_double_hash`: save/peek/peek/restore
    helper that returns true when the next two tokens are both
    `#` SYMBOL.
  - `preprocessor_macro_function_push_something` first checks
    for a trailing `##` and dispatches to handle_concat
    instead of the normal push-arg or push-verbatim path.
    Replaces the `#warning "process concat"` stub.
  - Forward decl for `preprocessor_macro_function_push_something`
    added (it's now called from concat_part above its
    definition).

Test: `tests/156-preprocessor-concat.sh` writes
`#define CAT(a, b) a ## b\nCAT(foo, bar);` and runs it through
the real lex + preprocessor pipeline. Confirms one
IDENTIFIER `foobar` lands in compiler->token_vec.
