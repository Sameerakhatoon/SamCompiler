# ch137 - starting to implement function code generation

`codegen_generate_root_node` now dispatches NODE_TYPE_FUNCTION into a
proper prologue / epilogue. The body emit is a stub for now.

What landed in `compiler.h`:
- `C_STACK_ALIGNMENT 16` + `C_ALIGN(size)` rounding macro.
- `IS_ALONE_STATEMENT` codegen history flag.
- `compile_process.resolver` field + forward decl for
  `struct resolver_process`.
- Forward decls for `function_node_is_prototype`,
  `function_node_stack_size`, `function_node_argument_vec`.

What landed in `node.c`:
- The three function_node accessors above.

What landed in `cprocess.c`:
- `compile_process_create` now also constructs a default resolver
  via `resolver_default_new_process(process)`.

What landed in `codegen.c`:
- File-scope `current_function` plus a local `struct history` with
  `codegen_history_begin` / `codegen_history_down`.
- `codegen_new_scope` / `_finish_scope` now defer to the default
  resolver.
- `asm_push_ins_push` / `asm_push_ins_pop`: emit `push` / `pop`
  asm and update the stack-frame ledger with a type+name. `pop`
  asserts the top element matches what's expected.
- `asm_push_ebp` / `asm_pop_ebp`: save / restore EBP with the
  matching ledger element `function_entry_saved_ebp`.
- `codegen_stack_sub_with_name` / `_add_with_name`: emit
  `sub esp, N` / `add esp, N` while pushing or popping
  `N/STACK_PUSH_SIZE` UNKNOWN ledger entries so the stackframe
  stays balanced.
- `codegen_new_scope_entity` / `codegen_register_function`:
  thin wrappers over the rdefault counterparts.
- `codegen_generate_function_prototype(node)`: emits `extern <name>`
  and registers the function.
- `codegen_generate_function_arguments(args_vec)`: registers each
  arg as a stack-resident scope entity at its `var.aoffset`.
- `codegen_generate_body(node, history)`: stub.
- `codegen_generate_function_with_body(node)`: registers the
  function, emits `global <name>`, the entry label, `push ebp` /
  `mov ebp, esp` / `sub esp, C_ALIGN(stack_size)`, opens a stack
  scope, registers args, calls into the body emitter, closes the
  scope, restores esp / ebp, asserts frame balance, emits `ret`.
- `codegen_generate_function(node)`: lazily allocates
  `node->func.frame.elements` (the per-function stack ledger),
  then dispatches prototype vs body.
- `codegen_generate_root_node` extended with the FUNCTION case.
  VARIABLE is a no-op here (data section already handled it).

Test: `tests/85-codegen-function-prologue.sh` compiles
`int main() { int a; int b; }` and grep-asserts the full
`global main: push ebp mov ebp, esp sub esp, 16 ... add esp, 16
pop ebp ret` sequence in the asm output.
