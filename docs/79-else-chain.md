# ch79 - implementing else and else-if

The `if`'s `.next` field now actually chains; both bare `else` and
the `else if` recursive form are supported.

What landed:

- `struct else_stmt { body_node }` added to the statement payload.
- `make_else_node(body)` in `node.c`.
- `token_next_is_keyword(keyword)` helper in `parser.c`.
- `parse_else(history)` - parse `{ ... }`, build a NODE_TYPE_STATEMENT_ELSE.
- `parse_else_or_else_if(history)`:
  - If the next token isn't `else`, return NULL (no chain).
  - Eat `else`. If `if` follows, recurse into `parse_if_stmt` and
    return that IF.
  - Otherwise parse the else body.
- `parse_if_stmt` now passes
  `parse_else_or_else_if(history)` as the `next` of `make_if_node`.

Smoke test (`tests/49-else-chain.sh`) feeds
`if(1) {} else if(2) {} else {}` and walks the chain:
IF -> IF (else-if) -> ELSE.
