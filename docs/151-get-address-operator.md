# ch151 - generating the get address operator

`&x` now compiles to `lea ebx, [addr] + push ebx`. Unary nodes in
expression position dispatch through a new `codegen_generate_unary`.

What landed in `codegen.c`:
- `codegen_gen_mem_access` checks `EXPRESSION_GET_ADDRESS` first;
  if set, delegates to `codegen_gen_mem_access_get_address`
  (already shipped in ch150).
- `codegen_generate_unary_address`: walks the operand with
  `EXPRESSION_GET_ADDRESS` so the recursive mem-access emits an
  address rather than a value, then acknowledges
  `RESPONSE_FLAG_UNARY_GET_ADDRESS`.
- `codegen_generate_unary(node, history)`: try
  `codegen_resolve_node_for_value` first; if it doesn't fire,
  dispatch by op. `*` is a stub for later; `&` -> address-of.
- `codegen_generate_expressionable` extended with the
  `NODE_TYPE_UNARY` case.

Test: `tests/98-codegen-address-of.sh` compiles
`int main() { int b; int* p = &b; }` and confirms
`lea ebx, [ebp-4]`, `push ebx`, and the resulting
`mov dword [ebp-8], eax` store into p.
