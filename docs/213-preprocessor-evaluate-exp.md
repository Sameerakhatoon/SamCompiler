# ch213 - evaluating the expressions in the preprocessor - part 4

Wires binary expression evaluation onto the preprocessor's
expression evaluator. After this, `#if 1 + 2`, `#if A == B`,
`#if A && B` etc. all evaluate correctly.

What landed:
- `compiler.h`: decl for `arithmetic(compiler, left, right, op,
  &success)`.
- `helper.c`: `arithmetic` body - a switch over the operator
  string covering `* / + - == != > < >= <= << >> && ||`.
  Returns the result, sets `*success = false` on unsupported.
- `preprocessor/preprocessor.c`:
  - `preprocessor_arithmetic` wraps `arithmetic` and calls
    `compiler_error` when success is false.
  - `preprocessor_evaluate_exp` evaluates left + right operands
    recursively (with TODO #warning commented for macro
    function call / tenary), passes to preprocessor_arithmetic.
  - `preprocessor_evaluate` switch gains EXPRESSION_NODE case.
- Upstream also fixed `assert(left_node_type = 0)` to `>= 0`
  in expressionable.c here; we already shipped that deviation
  back in ch194, so no change.

Test: `tests/142-preprocessor-evaluate-exp.sh` exercises
`#if 1 + 2`, `#if 1 - 1`, `#if 3 * 2`, `#if 5 > 2`, `#if 5 < 2`
and confirms each evaluates to the expected truthy/falsy
inclusion of the body.
