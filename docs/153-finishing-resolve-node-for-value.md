# ch153 - finishing the codegen_resolve_node_for_value function

`codegen_resolve_node_for_value` now post-processes the pushed
value based on its dtype, instead of trusting the entity-access
chain to leave the right shape on the stack.

What landed in `codegen.c`:
- After a successful `codegen_resolve_node_return_result`, peek
  the top ledger entry's dtype via `asm_datatype_back`.
- Struct / union (non-pointer): emit a `codegen_generate_structure_push`
  so the in-memory value becomes a real chunk-by-chunk push.
- Non-pointer primitive: pop into `eax`, apply final indirection
  via `mov eax, [eax]` if the resolver flagged it
  (`FINAL_INDIRECTION_REQUIRED_FOR_VALUE`), `codegen_reduce_register`
  to sign/zero-extend sub-DWORD reads, and push as the typed
  `result_value`.
- Pointers stay on the stack as-is.

Existing tests (`88-codegen-assign-expression`, `89-codegen-arithmetic`,
`92-codegen-identifier-load`, etc.) continue to pass, so the new
path is at least no regression on the primitive paths we already
exercise. Final-indirection coverage lands once we have an
expression that actually emits `DO_INDIRECTION` rules; we'll add a
dedicated test then.
