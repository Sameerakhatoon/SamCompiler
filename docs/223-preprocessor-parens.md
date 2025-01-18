# ch223 - implementing the parentheses node in the preprocessor

Wires the PARENTHESES_NODE case in `preprocessor_evaluate` so
`#if (1 + 2) > 0` etc. work.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_evaluate_parentheses(compiler, node)`: returns
  `preprocessor_evaluate(compiler, node->parenthesis.exp)`.
- `preprocessor_evaluate` switch gains `PREPROCESSOR_PARENTHESES_NODE`
  routing to `evaluate_parentheses`.

Test: `tests/153-preprocessor-evaluate-parens.sh` confirms
`#if (1 + 2) > 0` includes the body.
