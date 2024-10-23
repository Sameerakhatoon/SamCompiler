# ch171 - generating expression parenthesis

`(expr)` in expression position now emits real code by stripping
uninheritable flags and walking the inner expression.

What landed in `codegen.c`:
- `codegen_generate_exp_parenthesis_node(node, history)`: thin
  wrapper that calls `codegen_generate_expressionable(parenthesis.exp,
  history_down(history, codegen_remove_uninheritable_flags(flags)))`.
- `codegen_generate_expressionable` extended with the
  `NODE_TYPE_EXPRESSION_PARENTHESES` case.

Test: `tests/112-codegen-parens.sh` compiles
`int main() { int x; x = (50); }` and confirms the literal 50 is
pushed through the parens and stored at x.
