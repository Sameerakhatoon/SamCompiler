# ch81 - implementing the return statement

`return [expr];` parses to a `NODE_TYPE_STATEMENT_RETURN`.

What landed:

- `struct return_stmt { struct node* exp; }` added to `struct
  statement` (NULL exp = bare `return;`).
- `make_return_node(exp)` in `node.c`.
- `parse_return(history)` in parser.c: eats `return`, then either
  `;` (NULL exp) or `expr ;`.
- `parse_keyword` grows a `"return"` arm.

Smoke test (`tests/50-return.sh`) feeds
`int main() { return 42; }` and asserts the function body's first
statement is NODE_TYPE_STATEMENT_RETURN with `exp_val=42`.
