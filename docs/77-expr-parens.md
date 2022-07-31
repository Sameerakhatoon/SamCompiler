# ch77 - implementing expression parentheses

`(50 + 20)` parses to a real `NODE_TYPE_EXPRESSION_PARENTHESES`
wrapping the inner expression. `f(args)` reuses the same machinery,
turning into `NODE_TYPE_EXPRESSION { op="()", left=f, right=parens }`.

What landed:

- New node payload `struct parenthesis { struct node* exp; }` in the
  union.
- `make_exp_parentheses_node(exp)` in `node.c`.
- `node_is_expression_or_parentheses(n)` + `node_is_value_type(n)` -
  predicates used by the parser to decide whether the thing currently
  on the node stack is a callable expression.
- `parser.c::parser_blank_node` - a module-private NODE_TYPE_BLANK
  allocated once in `parse()` and used as the inner expression for
  empty `()`.
- `parser_deal_with_additional_expression()` - after a paren group
  closes, if the next token is an operator, keep parsing so e.g.
  `(50+20)+30` reduces to one expression.
- `parse_for_parentheses(history)`:
  1. Eat `(`.
  2. If a value-typed node is already on top of the stack, pop it as
     the "callee" (function-call shape).
  3. Parse the inner expression (or use BLANK for empty `()`).
  4. Eat `)`.
  5. Build a parens node; if there was a callee, build an
     `EXPRESSION("()" , callee, parens)`.
  6. Run `parser_deal_with_additional_expression()`.
- `parse_exp` dispatches: if next is `(`, call `parse_for_parentheses`;
  else `parse_exp_normal`.
- `parse()` allocates `parser_blank_node` once at start.

Smoke test (`tests/47-paren-expr.sh`) feeds `(50 + 20);` and asserts
the root is NODE_TYPE_EXPRESSION_PARENTHESES with the inner `+` op.
