# ch156 - generating statement return

`return [expr];` now emits real x86. The return value is placed in
`eax` (or copied into the caller's hidden struct slot at [ebp+8] for
struct returns), the function's local stack is freed, ebp is
restored, and `ret` fires.

What landed in `codegen.c`:
- `codegen_stack_add_no_compile_time_stack_frame_restore(N)`:
  emits `add esp, N` without touching the compile-time stack-frame
  ledger. Used by `return` because the ledger only balances at the
  function epilogue and we don't want to double-count.
- `asm_pop_ebp_no_stack_frame_restore`: emits `pop ebp` without
  touching the ledger.
- `codegen_generate_statement_return_exp(node)`: walks the return
  expression. Struct/union value -> `mov edx, [ebp+8]` then
  `codegen_generate_move_struct` into the caller's slot, then
  `mov eax, [ebp+8]` so the caller sees the pointer. Primitive ->
  pop into eax.
- `codegen_generate_statement_return(node)`: dispatch entry. Runs
  the exp emitter (if any), then the no-ledger stack restore and
  `ret`.
- `codegen_generate_statement` extended with
  `NODE_TYPE_STATEMENT_RETURN` and a `NODE_TYPE_UNARY` case so a
  bare unary in statement position dispatches correctly.

Test: `tests/100-codegen-return.sh` compiles
`int main() { int a; return 42; }` and checks the return-path
emits `push dword 42`, `pop eax`, `add esp, 16`, `pop ebp`, and at
least two `ret` instructions (the early return plus the
function epilogue).
