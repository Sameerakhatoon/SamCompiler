# ch13 - creating a keyword token

`token_make_identifier_or_keyword` now consults `is_keyword(spelling)`
before emitting the token. If the spelling matches the reserved-word
table (`int`, `long`, `if`, `for`, `struct`, etc.), it emits
`TOKEN_TYPE_KEYWORD`; otherwise it stays `TOKEN_TYPE_IDENTIFIER`. The
underlying char data is the same either way - only the `type` tag
changes.

Notable entry in the table: `"include"`. The operator dispatch from
ch10 uses `token_is_keyword(last, "include")` to decide whether `<`
opens a `<...>` string (as in `#include <stdio.h>`) or is just the
less-than operator.

Smoke test (`tests/11-keyword-tokens.sh`) feeds
`gerog erlgermo skgm5845 int long` and asserts three IDENTIFIERS and
two KEYWORDS, with `int` and `long` getting the keyword tag.
