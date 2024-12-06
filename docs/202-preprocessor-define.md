# ch202 - creating the define macro

First preprocessor directive lands: `#define`. The preprocessor
now switches on TOKEN_TYPE_SYMBOL, intercepts `#`, peeks the
next token, and if it's `define`, swallows the rest of the line
into a `preprocessor_definition` and registers it on
`compiler->preprocessor->definitions`.

What landed:
- `compiler.h`: `struct preprocessor_definition` gains a
  `preprocessor*` back-pointer so subsequent macro expansion
  has reach back into the owning preprocessor.
- `preprocessor/preprocessor.c`:
  - Token push helpers: `preprocessor_token_push_to_dst`,
    `preprocessor_token_push_dst`,
    `preprocessor_token_vec_push_src_to_dst`,
    `preprocessor_token_vec_push_src`,
    `preprocessor_token_vec_push_src_token`. All variations on
    "push tokens into the compiler->token_vec output stream"
    so the rest of the preprocessor can reuse them.
  - `preprocessor_is_preprocessor_keyword`: matches one of
    define / undef / warning / error / if / eleif (typo
    preserved) / ifdef / ifndef / endif / include / typedef.
  - `preprocessor_token_is_preprocessor_keyword`: returns true
    if token type is IDENTIFIER, or if KEYWORD and its sval is
    in the preprocessor-keyword set. Upstream missed the
    parentheses around the `&&` so `||` short-circuits any
    IDENTIFIER through; preserved verbatim.
  - `preprocessor_token_is_define`: gate + S_EQ "define".
  - `preprocessor_multi_value_insert_to_vector`: walks the
    original token stream pushing tokens onto a vector until
    it hits a NEWLINE; backslash + newline pairs are skipped
    so `#define X 1 + \\\n  2` works.
  - `preprocessor_definition_remove`: linear scan of
    preprocessor->definitions, vector_pop_last_peek's any
    entry matching the name.
  - `preprocessor_definition_create`: removes any existing
    definition with the same name, callocs a fresh one, sets
    type=STANDARD initially. If arguments vector is non-empty,
    bumps type to MACRO_FUNCTION. Pushes onto
    preprocessor->definitions.
  - `preprocessor_handle_definition_token`: reads name, builds
    an empty arguments vector (TODO #warning: macro args land
    next chapter), reads the value tokens via
    multi_value_insert_to_vector, calls definition_create.
  - `preprocessor_handle_hashtag_token`: peeks next token; if
    `define`, dispatches to handle_definition_token.
  - `preprocessor_handle_symbol`: if `#`, route to
    handle_hashtag_token; otherwise push through.
  - `preprocessor_handle_token` switch gains TOKEN_TYPE_SYMBOL
    + TOKEN_TYPE_NEWLINE (ignored) cases; default still
    push-through (ch200 deviation kept until full expansion).

Test: `tests/132-preprocessor-define.sh` hand-builds a token
stream for `#define FOO 42 \n`, runs preprocessor_run, and
confirms the preprocessor->definitions vector grows by one and
the new entry has name=FOO, type=STANDARD, back-pointer set,
and value-token vector containing one NUMBER(42).
