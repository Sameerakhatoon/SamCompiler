# ch162 - generating for statements (continue)

`continue;` now emits `jmp .entry_point_<id>`. The chapter name in
the book is misleading - the actual diff is `continue` statement
codegen plus a one-character typo fix on the for-loop end label
(book had `.for_loop_end%i` missing the `:`; our ch160 implementation
already shipped the colon).

What landed in `codegen.c`:
- `codegen_generate_continue_stmt(node)`: delegates to
  `codegen_goto_entry_point(node)`.
- `codegen_generate_statement` dispatches
  `NODE_TYPE_STATEMENT_CONTINUE`.

Known carry-over bug: inside a `for` loop, `continue` jumps back to
the start label which skips the incrementer expression. Book ships
a `#warning` about this; we preserve the same behavior and will
file a gotcha once we have a runtime test that exercises the
breakage end-to-end.

Test: `tests/106-codegen-continue.sh` compiles
`int main() { int x; for(x = 0; x < 50; x = x + 1) { continue; } return x; }`
and asserts `jmp .entry_point_` appears.
