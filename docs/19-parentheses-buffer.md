# ch19 - dealing with the parentheses buffer

`struct lex_process` has had a `parentheses_buffer` field since ch7;
ch10 allocates it lazily on the first `(`. This chapter finally writes
to it.

Two small hooks:

- `nextc` - while `lex_is_in_expression()` is true, every char we read
  also gets appended to `parentheses_buffer`.
- `token_create` - if we're inside an expression at token-creation
  time, stamp the token's `between_brackets` pointer to the current
  contents of `parentheses_buffer`.

The result: every token born inside `( ... )` carries a snapshot of
the raw substring read so far. That's enough later for diagnostics or
error messages to quote the original source verbatim.

(Note the asymmetry: the buffer is per-paren-pass and accumulates;
this is intentional. The book uses it to dump the source-form of an
expression on demand, not to bound each individual token's slice.)

Smoke test (`tests/16-parentheses-buffer.sh`) feeds `(50+20)` and
asserts the inner tokens carry a `between_brackets` field that
contains the substring read inside the parens.
