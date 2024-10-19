# ch166 - creating switch statements Part 3

`switch (exp) { case N: ... }` lands real codegen: jump-table at
the top dispatches to per-case labels inside the body.

What landed in `compiler.h`:
- `struct parsed_switch_case` moved out of `parser.c` so codegen
  can iterate the switch's case vector. Forward-removed from
  parser.c with a pointer to the new home.

What landed in `codegen.c`:
- `codegen_goto_exit_point_maintain_stack(node)`: currently the
  same as `codegen_goto_exit_point`; the book reserves the name
  for future stack maintenance during case-fallthrough.
- `codegen_generate_switch_default_stmt(node)`: emits the
  `.switch_stmt_<id>_case_default:` label.
- `codegen_generate_switch_stmt_case_jumps(node)`: walks the
  parsed cases, emitting `cmp eax, N` + `je
  .switch_stmt_<id>_case_N`. After all cases either jumps to the
  default label or to the exit point.
- `codegen_generate_switch_stmt(node)`: opens an entry/exit point
  + switch frame, walks the switch expression onto eax, emits the
  jump table, then the body, then closes the frame.
- `codegen_generate_statement` dispatches
  `NODE_TYPE_STATEMENT_SWITCH`.

Book quirk preserved (G06): the default-case branch passes
`codegen_switch_id` (the function pointer, no parentheses) to the
`%i` format slot, so the emitted default jump targets a bogus
truncated id. We wrap the bug verbatim with a cast to silence the
compiler warning; a real gotcha fix lands once we have a default-
case test that catches the runtime failure.

Test: `tests/108-codegen-switch.sh` compiles
`int main() { int x = 3; switch(x) { case 3: x = 90; break;
case 1: x = 20; break; } return x; }` and asserts the
`.switch_stmt_`, `cmp eax, 3`, `cmp eax, 1`, and
`je .switch_stmt_` jump-table sequence.
