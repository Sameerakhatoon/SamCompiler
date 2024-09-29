# ch157 - generating if statements

`if (...) {} else if (...) {} else {}` chains now emit real
conditional jumps + bodies + labels.

What landed in `codegen.c`:
- `codegen_generate_else_stmt(node)`: emits the body of an `else`
  block as a stand-alone body under a fresh history.
- `codegen_generate_else_or_else_if(node, end_label_id)`: recurses
  into a nested `if` (chained `else if`) or stops at a final `else`.
  Anything else is a compiler bug.
- `_codegen_generate_if_stmt(node, end_label_id)`: emits the
  condition expression, pops eax, `cmp eax, 0`, `je .if_<id>`,
  body, `jmp .if_end_<end_label_id>`, then the `.if_<id>:` label
  so the false branch lands there. If a `next` (else / else-if)
  exists, it gets emitted right after the `.if_<id>` label.
- `codegen_generate_if_stmt(node)`: allocates a shared
  `.if_end_<id>` label so every branch jumps to the same exit and
  drives `_codegen_generate_if_stmt`.
- `codegen_generate_statement` extended with
  `NODE_TYPE_STATEMENT_IF`.

Test: `tests/101-codegen-if.sh` compiles
`int main() { int x = 5; if (x) { return 1; } else { return 0; } }`
and asserts `cmp eax, 0`, `je .if_`, `jmp .if_end_`, and a
`.if_end_` label all appear.
