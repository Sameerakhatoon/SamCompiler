# ch31 - dealing with precedence in expressions (part 3)

Upstream this chapter is just a verification run on the existing
reorder code with a new input shape - `50*30+20`. No code changes;
the parser from ch30 already handles both directions.

For `50*30+20`:
- Left-to-right parsing builds `EXPRESSION(*, 50, 30)` first.
- Then the next operator `+` shows up. The existing left-associative
  rule (lower-precedence op on the right) means `+` gets the
  expression as its **left** child, with `20` as the leaf right.

So the AST root is `+`, left = `(50*30)`, right = `20`. Mirror image
of ch30's `1+2*3`. Our `parser_reorder_expression` already handles
this; ch31 is purely a coverage test.

Smoke test (`tests/27-precedence-reorder-mirror.sh`) asserts the
mirrored layout.
