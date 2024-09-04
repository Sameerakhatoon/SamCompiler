# ch140 - generating expressions

Assignment statements now emit code. Reads the LHS through the
resolver, computes the destination, and writes the popped RHS.

What landed in `codegen.c`:
- `codegen_generate_entity_access_start(result, root_entity, history)`:
  decide how to materialise the first entity of an LHS chain.
  - UNSUPPORTED -> recurse via `codegen_generate_expressionable`.
  - FIRST_ENTITY_PUSH_VALUE -> `push dword [base.address]`.
  - FIRST_ENTITY_LOAD_TO_EBX -> `lea ebx, [base]`
    (or `mov ebx, [base]` if the next entity is a pointer-array),
    then push ebx.
- `codegen_generate_entity_access_for_variable_or_general`: pop ebx,
  apply `mov ebx, [ebx]` for DO_INDIRECTION, add the entity's
  offset, push ebx back.
- `codegen_generate_entity_access_for_entity_for_assignment_left_operand`:
  dispatch by entity type (only VARIABLE / GENERAL fire today; the
  others are stubs with TODOs).
- `codegen_generate_entity_access_for_assignment_left_operand`:
  walk the entity chain through the access helpers.
- `codegen_generate_assignment_part(node, op, history)`:
  - Single-entity LHS: pop eax, emit `mov / add` directly into the
    resolved base address.
  - Multi-entity LHS: build the address into ebx via the access
    walker, pop edx (the address), pop eax (the value), then store
    via `[edx]`.
- `codegen_generate_assignment_expression`: walk RHS as an
  expressionable, then run `_assignment_part` on LHS.
- `codegen_generate_exp_node`: only assignment for now;
  arithmetic / comparison / call lands later.
- `codegen_generate_statement` extended with EXPRESSION case.

Test: `tests/88-codegen-assign-expression.sh` compiles
`int main() { int b = 50; b = 20; }` and asserts both the init
literal (50) and the reassign literal (20) get pushed, and that
the `mov dword [ebp-4], eax` store appears twice.
