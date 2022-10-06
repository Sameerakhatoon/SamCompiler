# ch148 - implementing function calls

Function call expressions now emit real cdecl-style call sequences,
and statements drain any leftover result values from the stack.

What landed in `stackframe.c`:
- `stackframe_peek_start` now walks top-down via
  `vector_set_peek_pointer_end` + `VECTOR_FLAG_PEEK_DECREMENT` so
  codegen scans the most recent pushes first.

What landed in `codegen.c`:
- `codegen_generate_entity_access_for_function_call(result, entity)`:
  - Iterates the entity's `func_call_data.arguments` vector from
    the end (cdecl pushes args right-to-left).
  - Pops the callee address into ebx, moves it into ecx (call
    target).
  - Walks each argument node through `codegen_generate_expressionable`
    with `EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS` set.
  - Emits `call ecx`.
  - Adds `func_call_data.stack_size` to esp (caller cleans args).
  - Pushes `eax` back as the next `result_value` carrying the
    function's return dtype.
- `codegen_generate_entity_access` acknowledges the resolved last
  entity to the parent response.
- Both LHS and read-side entity dispatch now route
  `RESOLVER_ENTITY_TYPE_FUNCTION_CALL` through the new helper.
- `codegen_discard_unused_stack`: walks the ledger top-down, summing
  consecutive `result_value` pushes (something computed but not
  consumed) and bumping esp past them.
- `asm_stack_peek` / `asm_stack_peek_start` thin wrappers around the
  stackframe helpers.
- `codegen_generate_statement` calls `_discard_unused_stack` after
  every statement.

Test: `tests/95-function-call-codegen.sh` compiles
`int puts(int x); int main() { puts(42); }` and confirms `extern
puts`, `push dword 42`, `call ecx`, and a stack-reclaim `add esp,`
all appear in the asm.
