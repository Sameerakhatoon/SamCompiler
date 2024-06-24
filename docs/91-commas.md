# ch91 - implementing commas

The comma operator `a, b` parses as a NODE_TYPE_EXPRESSION with op `","`.
Sequence semantics happen later at codegen; for the parser, it's just
a binary op.

What landed:
- `parse_for_comma(history)` in parser.c: eat the comma, pop the
  previous expression as left, parse the right, build an EXPRESSION
  with op `","`.
- `parse_exp` dispatch routes `,` to `parse_for_comma`.

(The book ships this as a low-precedence operator. We don't update
the precedence table - it's already in the lowest precedence group.)
