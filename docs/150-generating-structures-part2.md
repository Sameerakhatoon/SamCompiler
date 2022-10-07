# ch150 - generating structures part 2

Struct-by-value reads and writes now have real codegen: chunked
`STRUCTURE PUSH` reads via `lea ebx + push dword [ebx+N]` and
chunked `pop eax + mov [base+N], eax` writes.

What landed in `codegen.c`:
- `asm_push_ins_push_with_flags`: push variant that tags the
  ledger element with arbitrary flags (used to mark
  `IS_PUSHED_ADDRESS`).
- `codegen_plus_or_minus_string_for_value`: formats a chunk offset
  as `+N` or `-N`.
- `codegen_generate_structure_push`: walks struct dwords from
  highest offset to lowest, pushing each as `dword [ebx+N]` (the
  book's ch149 version used `i++` which would never terminate; we
  ship the ch150 `i--` from the start).
- `codegen_generate_structure_push_or_return`: thin wrapper that
  the caller uses regardless of context.
- `codegen_generate_move_struct(dtype, base_address, offset)`: pops
  struct dwords back off the stack and writes them via
  `mov [base+N], eax`.
- `codegen_gen_mem_access_get_address`: `lea ebx, [addr]` + push
  ebx tagged `IS_PUSHED_ADDRESS`.
- `codegen_gen_mem_access` extended: struct/union value path does
  get-address -> pop ebx -> structure_push.
- Function-call path:
  - Caller now subtracts space for a returned struct/union and
    pushes esp as the hidden first argument.
  - After the call, if the return is a struct, do `mov ebx, eax`
    and `codegen_generate_structure_push`. Else keep the existing
    `push eax`.
- Assignment-part single-entity path now calls
  `codegen_generate_move_struct` for struct LHS instead of falling
  through to the scalar mov.

Test: `tests/97-codegen-struct-byval.sh` compiles
`struct foo { int a; int b; }; struct foo x; struct foo y;
int main() { x = y; }` and confirms `; STRUCTURE PUSH`,
`; END STRUCTURE PUSH`, `lea ebx, [y...]`, `pop eax`, and
`mov [x...]` all appear.
