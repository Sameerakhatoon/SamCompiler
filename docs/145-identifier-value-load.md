# ch145 - generating identifier variable access

Identifiers in expression position now emit a direct value-load
rather than the address-arithmetic path used by assignment LHS.

What landed in `codegen.c`:
- `codegen_reduce_register(reg, size, is_signed)`: for sub-DWORD
  reads, follow the mem load with `movsx` / `movzx` so the stack
  always holds a full dword.
- `codegen_gen_mem_access(node, flags, entity)`: DWORD reads do
  `push dword [addr]` straight from memory; smaller types load
  through eax then sign/zero-extend, then push eax.
- `codegen_generate_variable_access_for_entity` /
  `codegen_generate_variable_access`: thin wrappers around
  `codegen_gen_mem_access` that thread history down.
- `codegen_generate_identifier(node, history)`: resolve the
  identifier via the resolver, emit the value-load, acknowledge
  the top response with RESPONSE_FLAG_RESOLVED_ENTITY +
  the resolved entity.
- `codegen_generate_expressionable` updated to route
  `NODE_TYPE_IDENTIFIER` to `_generate_identifier` instead of
  routing it back through `codegen_generate_exp_node`.

Test: `tests/92-codegen-identifier-load.sh` compiles
`int main() { int b = 50; int e = 20; b = e + 10; }` and confirms
the `e + 10` expression emits `push dword [ebp-8]` (the direct
value of `e`), `push dword 10`, the arithmetic add, and a final
store into `b`.
