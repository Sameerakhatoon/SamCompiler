# ch142 - codegen response system + expressions

Codegen gets a small "response" stack for upward signalling between
recursive emit calls, a real binary-arithmetic path, and a
read-side entity-access walker that lets bare identifiers resolve
to values on the stack.

What landed in `compiler.h`:
- `code_generator.responses` vector.
- `RESPONSE_FLAG_*` (ACKNOWLEDGED / PUSHED_STRUCTURE /
  RESOLVED_ENTITY / UNARY_GET_ADDRESS).
- `EXPRESSION_GEN_MATHABLE` mask: every op the arithmetic emitter
  knows about.
- `EXPRESSION_UNINHERITABLE_FLAGS` mask: flags that must NOT be
  passed down into a child expression.

What landed in `codegen.c`:
- `struct response_data` / `struct response`.
- `codegen_response_expect` / `_pull` / `_acknowledge` /
  `_acknowledged` / `_has_entity`. Book quirk preserved: the
  `acknowledged` check writes `res->flags && FLAG` (both truthy)
  instead of the bitwise check; harmless given how the rest of the
  code paths interact.
- `asm_stack_back` / `asm_datatype_back`: read the most-recent
  stack-frame ledger element and (optionally) its attached dtype.
- `codegen_generate_entity_access_for_entity`: read-side
  counterpart of the assignment-LHS version; same VARIABLE /
  GENERAL fast path, other kinds stubbed for later.
- `codegen_generate_entity_access`: walks the entity chain through
  the read-side helpers.
- `codegen_resolve_node_return_result`: drives `resolver_follow`,
  emits entity access if it succeeded, acknowledges the response.
- `codegen_resolve_node_for_value`: thin wrapper that returns
  whether the resolve produced a value.
- `codegen_set_flag_for_operator`: map `+`,`-`,`*`,`/`,`%` to
  the matching EXPRESSION_IS_* bit.
- `codegen_can_gen_math` / `codegen_remove_uninheritable_flags` /
  `get_additional_flags`: history-flag plumbing for the arithmetic
  recursion.
- `codegen_gen_math_for_value`: emit the actual `add`, `sub`,
  `imul`/`mul`, `idiv`/`div` (with cdq/xor edx, edx pre-divide).
- `codegen_generate_exp_node_for_arithmetic`: push left, push
  right, pop right -> ecx, pop left -> eax, run the chosen op,
  push the result.
- `codegen_generate_exp_node` (real impl, replacing the ch140
  stub): assignment -> assignment path; bare identifier / resolved
  expression -> entity-access path; otherwise -> arithmetic path.
- `codegen_generate_expressionable` extended with IDENTIFIER and
  EXPRESSION cases, both routed to `_exp_node`.

Test: `tests/89-codegen-arithmetic.sh` compiles
`int main() { int a = 3 + 4; }` and confirms the emit sequence:
`push dword 3`, `push dword 4`, `pop ecx`, `pop eax`,
`add eax, ecx`, `mov dword [ebp-4], eax`.
