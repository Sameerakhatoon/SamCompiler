# ch154 - finishing unaries

`-x`, `~x`, `*p`, `**p` etc. all emit real code now, and the
read-side + LHS entity-access chains both handle `UNARY_INDIRECTION`
and `UNARY_GET_ADDRESS`.

What landed in `codegen.c`:
- `CODEGEN_ENTITY_RULE_*` enum (struct-or-union-non-pointer,
  function-call, get-address, will-peek-at-ebx).
- `codegen_entity_rules(last_entity, history)`: classifies the
  trailing entity into rule flags used by the access dispatchers.
- `codegen_apply_unary_access(depth)`: emits a chain of
  `mov ebx, [ebx]` for `depth` dereferences.
- `codegen_generate_unary_indirection`: walks the operand with
  `EXPRESSION_GET_ADDRESS | EXPRESSION_INDIRECTION` to get its
  address, pops into ebx, applies `depth` (or `depth+1` if we're
  not already in get-address) dereferences, and finally reduces
  the register width via `codegen_reduce_register` if we've
  collapsed the full pointer chain.
- `codegen_generate_normal_unary`: `-` -> `neg`, `~` -> `not`,
  `*` -> indirection.
- `codegen_generate_unary` extended to dispatch both indirection
  and the normal-unary tail.
- `codegen_generate_entity_access_for_unary_indirection`:
  read-side dispatch on the access chain - pops the address,
  applies `depth` dereferences, pushes back with
  `IS_PUSHED_ADDRESS`.
- `codegen_generate_entity_access_for_unary_get_address`:
  read-side dispatch - pops the address, comments `; PUSH ADDRESS
  &`, pushes it back as a typed result_value.
- `codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand`:
  LHS counterpart - same idea but uses `depth - 1` dereferences
  because the trailing store handles the last one.
- `codegen_generate_entity_access_for_entity` and its LHS twin
  both gain UNARY_INDIRECTION / UNARY_GET_ADDRESS cases.

Book quirk repaired: in two of the new functions the book passes
`result` (a `resolver_result*`) where the signature wants a
`history*`. We pass the real history so the call type-checks; the
flag the helper inspects (`EXPRESSION_GET_ADDRESS`) is read from
history->flags either way.

Test: `tests/99-codegen-unary.sh` compiles
`int main() { int a = -5; int b = ~5; int* p; int e = *p; }` and
confirms `neg eax`, `not eax`, and `mov ebx, [ebx]` all appear in
the asm.
