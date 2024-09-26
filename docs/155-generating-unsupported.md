# ch155 - generating unsupported entities

UNSUPPORTED entities on the access chain now have a real codegen
path instead of being silent stubs.

What landed in `codegen.c`:
- `codegen_generate_entity_access_for_unsupported(result, entity)`:
  hands the wrapped node off to `codegen_generate_expressionable`
  with a fresh history. Used as a fallback for any entity the
  resolver flagged as UNSUPPORTED.
- Forward decl above the LHS dispatcher so both call sites compile.
- `codegen_generate_entity_access_for_entity_for_assignment_left_operand`
  and `codegen_generate_entity_access_for_entity` extended with
  the UNSUPPORTED case wired through the new helper.

The book's ch155 also corrects the `result` -> `history` argument
typo in `codegen_generate_entity_access_for_unary_indirection`; we
fixed that as part of ch154 so it's already on our tree.

Existing tests stay green; we'll add a dedicated unsupported-flow
test when a later chapter lands a constructable case that the
parser actually marks UNSUPPORTED.
