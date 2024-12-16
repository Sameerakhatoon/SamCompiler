# ch209 - evaluating expressions in the preprocessor - Part 1

First chunk of the `#if`-style expression evaluator. Adds the
plumbing that takes a token vector, runs the
`preprocessor_expressionable_config` over it, and walks the
resulting node tree. Only the NUMBER case is wired this
chapter; later parts handle identifiers / unaries /
expressions.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_evaluate_number(node)`: returns
  `node->const_val.llnum` cast to int.
- `preprocessor_evaluate(compiler, root_node)`: switch over
  `root_node->type`; PREPROCESSOR_NUMBER_NODE routes to
  `evaluate_number`, everything else falls through to result =
  0.
- `preprocessor_parse_evaluate(compiler, token_vec)`: makes a
  node vector, `expressionable_create` with the preprocessor's
  shipped config, `expressionable_parse`, pops the root, hands
  it to `preprocessor_evaluate`.

Test: `tests/139-preprocessor-evaluate.sh` builds a single-token
vector containing NUMBER(42) and confirms
`preprocessor_parse_evaluate` returns 42.
