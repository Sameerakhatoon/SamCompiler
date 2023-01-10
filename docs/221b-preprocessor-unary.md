# ch221b - implementing preprocessor unary not

Upstream reuses lecture number 221 (also macro strings part 1).
We slot this as ch221b. Wires the
`PREPROCESSOR_UNARY_NODE` case in `preprocessor_evaluate` so
unary operators in `#if` expressions evaluate.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_evaluate_unary`: switch over the unary op:
  - `!` -> logical not of the operand value.
  - `~` -> bitwise not.
  - `-` -> arithmetic negation.
  - anything else -> compiler_error.
- `preprocessor_evaluate` switch gains
  `PREPROCESSOR_UNARY_NODE` case.

Test: `tests/151-preprocessor-evaluate-unary.sh` exercises
`#if !0` (body included), `#if !1` (body skipped), and `#if -1`
(body skipped because read_to_end_if uses > 0).
