# ch176 - fixing a bug with indirection assignments

`*p = 50;` was getting miscompiled because the unary operand
parser kept eating operators (including `=`) past the actual
operand. Fix: tag the recursive descent with `EXPRESSION_IS_UNARY`
and stop the inner operator parser as soon as the next token isn't
a continuation (access, array bracket, or call parens).

What landed in `helper.c`:
- `is_parentheses(op)`: predicate for `(`.
- `unary_operand_compatible(token)`: true iff the next token's op
  is `.` / `->` / `[]` / `(`.

What landed in `parser.c`:
- `parse_for_indirection_unary` and `parse_for_normal_unary`
  descend with `history_begin(EXPRESSION_IS_UNARY)` instead of 0.
- `parse_exp` bails (returns -1) when both the unary flag is set
  AND the next token isn't unary-operand-compatible. Outer
  expression parser then resumes normally.
- `parse_expressionable_single` forwards `parse_exp`'s return so
  the bail-out is observable.

Test: `tests/116-indirection-assign.sh` compiles
`int main() { int* p; *p = 50; }` and confirms the asm has a
`push dword 50` plus an indirect store via `[edx]` or `[ebx]`.
