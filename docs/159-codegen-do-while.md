# ch159 - generating do while loops

`do { body } while (cond);` emits the body first, then the cond,
and `jne` loops back when the cond is non-zero.

What landed in `codegen.c`:
- `codegen_generate_do_while_stmt(node)`:
  - `codegen_begin_entry_exit_point()` to give break/continue a
    matched pair.
  - `.do_while_start_N:` label.
  - Body under `IS_ALONE_STATEMENT`.
  - Condition expressionable -> pop eax -> `cmp eax, 0` ->
    `jne .do_while_start_N`.
  - `codegen_end_entry_exit_point()`.
- `codegen_generate_statement` dispatches `NODE_TYPE_STATEMENT_DO_WHILE`.

Test: `tests/103-codegen-do-while.sh` compiles
`int main() { int x = 0; do { x += 1; } while(x < 5); return x; }`
and asserts both `.do_while_start_` and `jne .do_while_start_`
appear.
