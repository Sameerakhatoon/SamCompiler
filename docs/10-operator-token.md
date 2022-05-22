# ch10 - creating an operator token

This is where the lexer starts taking shape. New machinery in `lexer.c`:

- `OPERATOR_CASE_EXCLUDING_DIVISION` (in `compiler.h`) - the switch case
  that names every single char that can start an operator, except `/`
  (kept aside because the same path will later peel comments off).
- `op_treated_as_one(op)` - chars that never combine with a follow-up
  char into a longer operator (`(`, `[`, `,`, `.`, `*`, `?`).
- `is_single_operator(op)` - chars that can show up as the second char
  of a two-char operator.
- `op_valid(op)` - whitelist of every operator spelling we accept.
- `read_op` - eats one char, then greedily eats one more if the first
  wasn't "treated as one". If the resulting two-char operator isn't
  valid, push the second char back to the input stream and truncate to
  the first.
- `read_op_flush_back_keep_first` - the "push the tail back" helper.
- `lex_new_expression` / `lex_is_in_expression` - the paren-depth
  counter bumps on '(' and (later) drops on ')'. The first '('
  allocates `parentheses_buffer` so later chapters can populate the
  `between_brackets` field on tokens.
- `token_make_operator_or_string` - the dispatch entry. Two reasons it
  has "or_string" in its name:
  1. `<` after the `include` keyword opens a `<...>` string literal,
     not a less-than op, so we peek at the previous token to choose.
  2. `(` also calls `lex_new_expression` to bump the paren counter.

New files:

- `token.c` (declared in `compiler.h`) - holds
  `token_is_keyword(token, value)` so the operator dispatch can detect
  `#include <...>`.

`test.c` is now `50+20+50+39+28+18*5  ++` to exercise the mix.

Smoke test (`tests/08-operator-tokens.sh`) builds a probe that drives
the lexer on that input and asserts 14 tokens, with the expected NUMBER
/ OPERATOR mix, including the greedy `++` at the end.
