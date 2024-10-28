# ch175 - generating array access Part 2

Tightens the ch101 expression reorder condition so it doesn't fire
on `a[i] + b`-style expressions (which had been getting their
operands shuffled in a way that broke the array-bracket codegen
emitted in ch174).

What landed in `parser.c`:
- `parser_reorder_expression` extra-reorder check changes from:
  ```
  (is_array_node(left) || is_node_assignment(right))
  || (left=="()" && right==",")
  ```
  to:
  ```
  (is_array_node(left) && is_node_assignment(right))
  || ((left=="()" || left=="[]") && right==",")
  ```
- Net effect: `a[i] = b` still gets the move-right-left-to-left
  shuffle; `a[i] + b` does not; and the `,` case now covers
  `f(a[i], b)` calls as well as `f(a, b)`.

Existing tests stay green - we don't have a dedicated expression-
ordering test, but the array-access end-to-end test
(`tests/115-codegen-array-access.sh`) keeps passing, which is
exactly the case this fix is meant to protect.
