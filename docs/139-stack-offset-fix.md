# ch139 - fixing the parser_scope_offset_for_stack function

`var.aoffset` now correctly folds the variable's `padding` into the
recorded stack offset, and struct/union locals get padding when
their body is `padded`.

What landed in `parser.c`:
- `parser_scope_offset_for_stack`:
  - After the existing primitive-padding branch, a new check: if the
    variable is a struct or union whose body is padded, compute a
    padding to the next DWORD boundary.
  - `aoffset = offset + (upward ? padding : -padding)` instead of
    just `offset`. Downward-growing stacks (locals) subtract the
    padding; upward (args) add.

Test: `tests/87-stack-offset-padded.sh` parses
`int main() { int a; char b; int c; }` and confirms the offsets are
`a = -4`, `b = -5`, `c = -12` (the trailing int gets padded up from
-10 to -12 for natural alignment).
