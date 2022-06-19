# ch32 - creating an identifier node

Tiny refactor: split the `TOKEN_TYPE_IDENTIFIER` case in
`parse_expressionable_single` out into a dedicated
`parse_identifier(history)` function. The body is just
`parse_single_token_to_node()`, but the function exists so later
chapters can grow it (e.g. structure-member access via `a.b`).

**Bug shipped verbatim:** the book's assert in `parse_identifier` is

```c
assert(token_peek_next()->type == NODE_TYPE_IDENTIFIER);
```

`token->type` carries a `TOKEN_TYPE_*` value, not a `NODE_TYPE_*` value
(NODE_TYPE_IDENTIFIER == 3, TOKEN_TYPE_IDENTIFIER == 0). This assert
fires every time. Fix lands in **g01** as a follow-up commit.

Tests are intentionally red between ch32 and g01.
