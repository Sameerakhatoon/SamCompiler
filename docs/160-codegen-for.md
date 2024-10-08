# ch160 - generating for loops

`for (init; cond; loop) { body }` emits the canonical pattern;
any of the four parts may be missing.

What landed in `codegen.c`:
- `asm_push_ins_pop_or_ignore`: pop variant that no-ops when the
  top stack-frame element isn't the expected
  PUSHED_VALUE/"result_value". Used by the for-loop init / cond /
  loop expressionables that may or may not actually push.
- `codegen_generate_for_stmt(node)`:
  - `codegen_begin_entry_exit_point()`.
  - Init expressionable + ignore-or-pop eax.
  - `.for_loop<N>:` label.
  - Cond expressionable + ignore-or-pop eax + `cmp eax, 0` +
    `je .for_loop_end<M>`.
  - Body under `IS_ALONE_STATEMENT`.
  - Loop expressionable + ignore-or-pop eax.
  - `jmp .for_loop<N>` + `.for_loop_end<M>:` label.
  - `codegen_end_entry_exit_point()`.
- `codegen_generate_statement` dispatches
  `NODE_TYPE_STATEMENT_FOR`.

Test: `tests/104-codegen-for.sh` compiles
`int main() { int sum = 0; int i; for(i = 0; i < 10; i = i + 1) { sum = sum + i; } return sum; }`
and asserts `.for_loop`, `je .for_loop_end`, `jmp .for_loop`, and a
matching `.for_loop_end` label all appear.
