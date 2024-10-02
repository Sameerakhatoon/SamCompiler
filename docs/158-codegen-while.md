# ch158 - generating while loops

`while (cond) { body }` emits the canonical loop pattern with a
matched entry/exit point so `break` and `continue` can later hook
into the same labels.

What landed in `codegen.c`:
- `codegen_generate_while_stmt(node)`:
  - `codegen_begin_entry_exit_point()` to register a break/continue
    target pair.
  - `.while_start_N:` label.
  - Condition expressionable -> pop eax -> `cmp eax, 0` ->
    `je .while_end_M`.
  - Body emitted under an `IS_ALONE_STATEMENT` history.
  - `jmp .while_start_N`.
  - `.while_end_M:` label.
  - `codegen_end_entry_exit_point()` to close the pair.
- `codegen_generate_statement` dispatches `NODE_TYPE_STATEMENT_WHILE`.

Test: `tests/102-codegen-while.sh` compiles
`int main() { int x = 0; while(x < 50) { x += 1; } return x; }`
and asserts `.while_start_`, `je .while_end_`, `jmp .while_start_`,
and a matching `.while_end_` label all appear.
