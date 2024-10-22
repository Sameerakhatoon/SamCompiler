# ch168 / ch169 - generating goto and labels

`goto LABEL;` emits `jmp label_<name>`. The matching `LABEL:`
statement emits `label_<name>:` in asm. Both bodies are one-liner
codegen helpers.

What landed in `codegen.c`:
- `codegen_generate_goto_stmt(node)`: `jmp label_<node->stmt._goto.label->sval>`.
- `codegen_generate_label(node)`: `label_<node->stmt.label.name->sval>:`.
- `codegen_generate_statement` dispatches `NODE_TYPE_STATEMENT_GOTO`
  and `NODE_TYPE_LABEL`.

Test: `tests/110-codegen-goto-label.sh` compiles
`int main() { goto abc; return 0; }` and asserts
`jmp label_abc` appears. The matching `LABEL:` form needs a
parser tweak (statement-position symbol parsing) before we can
exercise the label emitter in a probe.
