# ch9 - creating a string token

Added `token_make_string(start_delim, end_delim)` and a `case '"'` in the
read_next_token dispatch. Consumes the opening delimiter (asserted),
then accumulates characters into a fresh buffer until the matching
closing delimiter or EOF, and emits a `TOKEN_TYPE_STRING` whose `sval`
points at the buffer's data.

Escape handling is deliberately stubbed for now: a `\\` byte is skipped
with a `continue`, no decoding. The full table (`\n`, `\t`, `\x..`,
etc.) lands later in the lexer-finalisation chapter.

`test.c` is now `"hello" 5838 "abnc494"` so `./main` exercises the
mixed string + number stream.

Smoke test (`tests/07-string-tokens.sh`) builds a probe that drives the
lexer on that input and asserts three tokens: STRING("hello"),
NUMBER(5838), STRING("abnc494").
