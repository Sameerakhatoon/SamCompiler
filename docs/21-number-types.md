# ch21 - creating number types

`struct token` for NUMBER tokens now records a subtype:
- `NUMBER_TYPE_NORMAL` (no suffix)
- `NUMBER_TYPE_LONG` (`L` suffix)
- `NUMBER_TYPE_FLOAT` (`f` suffix)
- `NUMBER_TYPE_DOUBLE` (placeholder for `d`)

Two pieces:

- New nested struct `struct token_number { int type; } num;` on every
  token. Only meaningful when `type == TOKEN_TYPE_NUMBER`.
- New enum `NUMBER_TYPE_*` in `compiler.h`.
- `lexer_number_type(c)` peeks; if it's an L/f, return the matching
  enum; otherwise NORMAL.
- `token_make_number_for_value` peeks for the suffix, consumes it if
  present, and stamps `num.type` accordingly.

Smoke test (`tests/18-number-types.sh`) feeds `42 5837L 7f` and
asserts the three NUMBER tokens get nt=0, nt=1, nt=2.

(`d` for double isn't wired into `lexer_number_type` yet; book follows
later.)
