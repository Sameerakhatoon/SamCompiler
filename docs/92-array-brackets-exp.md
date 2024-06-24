# ch92 - implementing array brackets in expressions

`arr[index]` is now parsable as an expression (in addition to the
declarator form from ch44).

What landed in `parser.c`:
- `parse_for_array(history)`:
  1. Pop the array operand (left).
  2. Eat `[`.
  3. Parse the index expression.
  4. Eat `]`.
  5. Build NODE_TYPE_BRACKET(inner=index).
  6. Build EXPRESSION(op="[]", left=arr, right=bracket).
- `parse_exp` dispatch: route `[` to `parse_for_array`.

No new test; existing expression parsing exercises the path.
