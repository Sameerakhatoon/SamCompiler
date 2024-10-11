# ch161 - generating break statements

`break;` now emits `jmp .exit_point_<id>`, hopping to the
innermost loop's exit label registered via
`codegen_begin_entry_exit_point`.

What landed in `codegen.c`:
- `codegen_generate_break_stmt(node)`: one-liner delegating to
  `codegen_goto_exit_point(node)` (the ch108 label-system helper
  that walks to the current exit point and emits the jump).
- `codegen_generate_statement` dispatches
  `NODE_TYPE_STATEMENT_BREAK`.

Test: `tests/105-codegen-break.sh` compiles
`int main() { int x; for(x = 0; x < 50; x = x + 1) { break; } return x; }`
and asserts `jmp .exit_point_` appears.
