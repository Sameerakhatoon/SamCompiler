# ch172 - creating tenary nodes

`cond ? T : F` emits the canonical ternary diamond. Book-typo
"tenary" preserved.

What landed in `codegen.c`:
- `codegen_generate_tenary(node, history)`:
  - Pop the cond (already pushed by the expression dispatcher) into
    eax, cmp eax, 0, je .tenary_false_<id>.
  - .tenary_true_<id>: emit the true branch, pop its
    result_value (or ignore), jmp .tenary_end_<id>.
  - .tenary_false_<id>: emit the false branch, pop its
    result_value (or ignore).
  - .tenary_end_<id>:.
- `codegen_generate_expressionable` extended with the
  `NODE_TYPE_TENARY` case.

Test: `tests/113-codegen-ternary.sh` compiles
`int main() { int x; x = 50 ? 10 : 87; return x; }` and asserts
`.tenary_true_`, `.tenary_false_`, `je .tenary_false_`, and
`jmp .tenary_end_` all appear.
