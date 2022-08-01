# ch78 - implementing IF statements

The parser now handles `if (cond) <body>`. ch79 will follow with the
`else if` / `else` chain.

What landed:

- `struct statement { struct if_stmt { cond_node, body_node, next }
  if_stmt; } stmt` added to the node union.
- `make_if_node(cond, body, next)` in `node.c`.
- `expect_keyword(keyword)` helper in `parser.c` (consume + assert).
- `parse_if_stmt(history)`:
  1. eat the `if` keyword.
  2. `(` ... expression ... `)`.
  3. parse_body for the consequent.
  4. `make_if_node(cond, body, NULL)` - `next` is filled by ch79's
     `parse_else` chain.
- `parse_keyword` dispatch grows an `if` arm.

Smoke test (`tests/48-if-statement.sh`) feeds
`int main() { if(1) { int y = 20; } }` and asserts the function body's
first statement is a NODE_TYPE_STATEMENT_IF whose cond is NUMBER(1).
