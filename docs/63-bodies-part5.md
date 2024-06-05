# ch63 - implementing bodies (part 5)

Wires the brace body into the top-level parser dispatch.

What landed in `parser.c`:

- `parse_symbol()` grows a `{` arm: calls `parse_body(...)` with the
  `HISTORY_FLAG_IS_GLOBAL_SCOPE` flag, pops the produced body node,
  and pushes it back as the top-level result. Everything else still
  errors.
- `parse_next` switch gains a `case TOKEN_TYPE_SYMBOL: parse_symbol();`
  arm.

Smoke test (`tests/42-brace-body.sh`) feeds `{ int x; int y; }` and
asserts a single NODE_TYPE_BODY root with 2 statements.
