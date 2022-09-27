# ch138 - implementing the foundations of scope variables

Codegen now walks function bodies and emits real stores for
initialized local variables. Numeric expressions land too.

What landed in `compiler.h`:
- Big `EXPRESSION_*` flag enum that ch138 onward threads through
  `codegen_history`. `IS_ALONE_STATEMENT` slots into the same enum.
- `datatype_for_numeric()` decl.

What landed in `helper.c`:
- `datatype_for_numeric()`: returns the canonical `int` literal
  datatype (DWORD, IS_LITERAL flag).

What landed in `codegen.c`:
- `asm_push_ins_push_with_data`: like the ch137 push variant but
  also stamps a `stack_frame_data` payload (dtype + flags) onto the
  pushed ledger element. Sets `HAS_DATATYPE`.
- `codegen_entity_private` / `codegen_sub_register` /
  `codegen_byte_word_or_dword_or_ddword`: low-byte / word / dword /
  ddword aliases for the 4 GP registers + the matching NASM size
  keyword.
- `codegen_generate_number_node`: emits `push dword <N>` and pushes
  a PUSHED_VALUE / "result_value" ledger element with the numeric
  flag.
- `codegen_generate_expressionable`: dispatch by node type. Sets
  `EXPRESSION_IS_NOT_ROOT_NODE` on entry so nested calls know
  they're past the top.
- `codegen_generate_assignment_instruction_for_operator`: `=` ->
  `mov`, `+=` -> `add`. Future chapters extend.
- `codegen_generate_scope_variable`: register the var as a stack
  entity, evaluate RHS via `_expressionable`, pop into `eax`, pick
  the size / register-alias / mov-type keyword, emit the store.
- `codegen_generate_statement`: dispatch (only VARIABLE for now).
- `codegen_generate_scope_no_new_scope` / `_stack_scope`: walk a
  body's statement vector under a fresh stack-scope.
- `codegen_generate_body` is now a one-liner into `_stack_scope`.

Test: `tests/86-codegen-local-assign.sh` compiles
`int main() { int a = 50; }` and confirms the asm has
`push dword 50`, `pop eax`, and `mov dword [ebp-4], eax`.
