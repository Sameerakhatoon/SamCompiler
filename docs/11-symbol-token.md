# ch11 - creating a symbol token

Added the SYMBOL category: `{`, `}`, `:`, `;`, `#`, `\`, `)`, `]`. Each
becomes a `TOKEN_TYPE_SYMBOL` carrying the raw char in `cval`.

Notable: `)` and `]` live in `SYMBOL_CASE`, not `OPERATOR_CASE`. That's
because seeing `)` has a side effect - drop the paren counter. The new
`lex_finish_expression` decrements `current_expression_count` and
`compiler_error`s if it goes negative (a `)` without a matching `(`).

So the symmetry is:
- `(` is an operator that bumps the counter (via `lex_new_expression`).
- `)` is a symbol that drops the counter (via `lex_finish_expression`).

That asymmetry is intentional - it lets us cheaply tell "this token
opens / closes an expression" without re-examining the cval.

Smoke test (`tests/09-symbol-tokens.sh`) feeds
`50+20+50+39+28+18*5  ++ (50+20) [#]` and asserts the three SYMBOL
tokens we expect: `)`, `#`, `]`. The `(` and `[` are operators per
ch10's `OPERATOR_CASE`.
