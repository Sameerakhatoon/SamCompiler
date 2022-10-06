# ch147 - implementing logical operators

`&&` / `||` short-circuit codegen + the rest of the comparison /
bitshift / bitwise operator dispatch.

What landed in `compiler.h`:
- `EXPRESSION_IN_LOGICAL_EXPRESSION` removed from
  `EXPRESSION_UNINHERITABLE_FLAGS` so it propagates into nested
  logical sub-expressions.
- Forward decls for `is_logical_operator` / `is_logical_node`.

What landed in `helper.c`:
- `is_logical_operator(op)` / `is_logical_node(node)` for
  `&&` / `||`.

What landed in `codegen.c`:
- `struct history_exp { logical_start_op, logical_end_label[20],
   logical_end_label_positive[20] }` plus a union in
  `struct history` so logical bookkeeping rides along.
- `codegen_set_flag_for_operator` extended with `>`, `<`, `>=`,
  `<=`, `!=`, `==`, `&&`, `<<`, `>>`, `&`, `|`, `^`.
- `codegen_gen_cmp(value, set_ins)`: `cmp eax, <v>; <set> al;
  movzx eax, al`.
- `codegen_gen_math_for_value` extended with MODULAS, ABOVE / BELOW
  / EQ / NEQ / ABOVE_OR_EQ / BELOW_OR_EQ (each routed through
  `codegen_gen_cmp`), BITSHIFT_LEFT (sal), BITSHIFT_RIGHT (sar),
  BITWISE_AND (and), BITWISE_OR (or), BITWISE_XOR (xor).
- `codegen_setup_new_logical_expression`: allocates an
  `.endc_<n>` / `.endc_<n>_positive` label pair via
  `codegen_label_count`.
- `codegen_generate_logical_cmp_and` / `_or` /
  `codegen_generate_logical_cmp`: emit `cmp <reg>, 0` and the
  short-circuit `je` / `jg` to the chosen label.
- `codegen_generate_end_labels_for_logical_expression`: emits the
  shared epilogue (move 1 on success, xor on fail for `&&`;
  mirror for `||`).
- `codegen_generate_exp_node_for_logical_arithmetic`: drives the
  left -> short-circuit cmp -> right -> conditional end-clause
  flow, sharing labels across nested logical nodes via the
  IN_LOGICAL_EXPRESSION flag.
- `codegen_generate_exp_node_for_arithmetic`: dispatches to the
  logical path when op is `&&` / `||`.

Test: `tests/94-logical-and-codegen.sh` compiles
`int main() { int a; int b; int r = a && b; }` and confirms the
`; && END CLAUSE` comment, the `cmp eax, 0`, the `je .endc_<n>`
short-circuit, the `mov eax, 1` success branch and the
`xor eax, eax` fail branch all appear.
