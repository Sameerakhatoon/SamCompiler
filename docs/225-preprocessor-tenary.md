# ch225 - evaluating tenaries in the preprocessor

Drops the placeholder `#warning "handle tenary node"` from
`preprocessor_evaluate_exp`. When the right operand of a binary
expression is a TENARY_NODE, the evaluator now branches on the
left operand's truthiness and evaluates the corresponding
true/false sub-node.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_evaluate_exp` TENARY arm:
  - if `left_operand` is truthy ->
    `preprocessor_evaluate(compiler, right->tenary.true_node)`.
  - otherwise ->
    `preprocessor_evaluate(compiler, right->tenary.false_node)`.

Test: `tests/155-preprocessor-tenary.sh` exercises `#if 1 ? 2 :
0` (body included) and `#if 0 ? 2 : 0` (body skipped).
