# ch28 - creating an expression node

The parser now handles binary expressions. `58272+2000` becomes a
single `NODE_TYPE_EXPRESSION` whose `.exp` carries `left`, `right`,
`op`.

New machinery:

- `struct exp { left, right, op }` inside `struct node`'s composite
  union (in `compiler.h`).
- `NODE_FLAG_INSIDE_EXPRESSION` flag (compiler.h) - sticky tag the
  parser drops on every node that's part of an expression subtree.
- `make_exp_node(left, right, op)` in node.c.
- `node_is_expressionable` / `node_peek_expressionable_or_null` in
  node.c - "is this a thing that can appear inside an expression?"
  (number / string / identifier / unary / expression / parens-exp).
- `struct history { int flags; }` in parser.c with
  `history_begin(flags)` and `history_down(history, flags)`. Threaded
  through every parse_* call so children inherit context (today just
  `INSIDE_EXPRESSION`); deeper modules will pile more flags on.
- `parse_expressionable_single` dispatch:
  NUMBER/IDENTIFIER/STRING -> single-token node; OPERATOR ->
  `parse_exp`.
- `parse_exp_normal` does the actual assembly: pop left, consume op,
  parse right, `make_exp_node`, re-push.
- Precedence-aware reordering is **not** here yet; that's ch29-31.

Smoke test (`tests/24-expression-node.sh`) feeds `58272+2000` and
asserts one root of `NODE_TYPE_EXPRESSION` (== 0 in the enum) with
op="+", left=58272, right=2000.
