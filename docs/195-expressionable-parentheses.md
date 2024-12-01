# ch195 - creating the expressionable system - Part 4

Adds parentheses parsing to the expressionable system. With this
the loop can now handle `(a + b) * c` and `f(x)`-style call
expressions through the generic machinery.

What landed in `compiler.h`:
- `bool is_operator_token(struct token* token)` decl.
- New `make_parentheses_node` slot in `struct
  expressionable_callbacks`.

What landed in `token.c`:
- `is_operator_token`: returns true if token type is
  TOKEN_TYPE_OPERATOR. Used by parentheses post-processing to
  decide whether more input should be parsed as a continuation.

What landed in `expressionable.c`:
- `expressionable_generic_type_is_value_expressionable(type)`:
  true for NUMBER / IDENTIFIER / UNARY / PARENTHESES /
  EXPRESSION. Used to decide whether a prior node on the stack
  should be treated as the left operand of a call-style `(`.
- `expressionable_expect_op(op)` / `expressionable_expect_sym(c)`:
  pop a token and assert it matches. Stub error path is
  `expressionable_error` from ch193.
- `expressionable_deal_with_additional_expression`: if the next
  token is an OPERATOR, recursively call `expressionable_parse`
  so an outer expression like `(a + b) * c` can continue past
  the closing `)`.
- `expressionable_parse_parentheses`: if the stack top is a
  value-expressionable node, treat it as the left operand of a
  call-style `(` (later building `left ()` parentheses
  expression). Expect `(`, parse inner, expect `)`, call
  `make_parentheses_node(exp)`. If we had a left operand, build
  the outer `make_expression_node(left, parens, "()")`. Finally
  fall into `deal_with_additional_expression` to keep parsing
  if the next token is an operator.

Wired:
- `parse_for_operator`'s `(` branch now calls
  `parse_parentheses` (replacing the placeholder warning).
- `parse_exp` now first checks for a leading `(` and dispatches
  to `parse_parentheses`.
- The custom-operator path in `parse_single_with_flags` now
  invokes the real `parse_exp` (replacing the placeholder
  warning).

Deviation from upstream: `parse_exp` falls through to
`parse_for_operator` unconditionally in the book, which
null-derefs `op_token->sval` for an input like `( 1 ) + 2`:
parse_parentheses + deal_with_additional_expression already
consumed the trailing `+ 2`, so by the time parse_exp's
fall-through runs, peek is NULL. Guarded here with an explicit
non-null + OPERATOR check.

Test: `tests/126-expressionable-parentheses.sh` feeds `( 1 + 2
) * 3` and confirms `make_parentheses_node` fires, the root
expression is `*`, its left child is a parentheses node, and
its right child is NUMBER 3.
