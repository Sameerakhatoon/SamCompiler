# ch222 - fixing a mistake with logical not in the code generator

Adds the missing `!` arm to `codegen_generate_normal_unary` in
`codegen.c`. Before this, `return !x;` would silently fall
through the unary chain with the unmodified operand on the
stack.

What landed in `codegen.c`:
- New `else if (S_EQ(node->unary.op, "!"))` branch that emits:
  - `cmp eax, 0`
  - `sete al`
  - `movzx eax, al`
  - then `asm_push_ins_push_with_data` so the canonical 0/1
    result is pushed as the unary expression's value.

Test: `tests/152-codegen-logical-not.sh` compiles `int main() {
return !0; }` and confirms the emitted asm contains both
`cmp eax, 0` and `sete al`.
