# ch12 - creating an identifier token

The dispatch switch now has a default catch-all that calls
`read_special_token`. Today it only knows about identifiers - if the
peeked char is a letter or `_`, eat the full identifier rune
([A-Za-z_][A-Za-z0-9_]*) into a buffer and emit a
`TOKEN_TYPE_IDENTIFIER`.

`token_make_identifier_or_keyword` lives in lexer.c and intentionally
has "or_keyword" in its name; ch13 will check the spelling against a
reserved-word table and re-tag it as `TOKEN_TYPE_KEYWORD` when it
matches.

Smoke test (`tests/10-identifier-tokens.sh`) feeds
`gerog erlgermo skgm5845` and asserts three identifier tokens with
those exact spellings. The `5845` digits don't split off `skgm5845`
because the identifier predicate accepts trailing digits.
