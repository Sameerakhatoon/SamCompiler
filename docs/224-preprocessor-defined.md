# ch224 - implementing joined nodes in the preprocessor

Wires the `JOINED_NODE` case in `preprocessor_evaluate`. The
joined-node was introduced in the expressionable callbacks so
that `defined NAME` could parse as two tokens linked together
(keyword + identifier). This chapter teaches the evaluator how
to interpret that pairing - currently only `defined IDENTIFIER`
is recognized; other joined forms evaluate to 0.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_pull_string_from(node)`: recursive helper -
  PARENTHESES recurses into `parenthesis.exp`, KEYWORD /
  IDENTIFIER return `sval`, EXPRESSION recurses into
  `exp.left`. Anything else returns NULL.
- `preprocessor_pull_defined_value(compiler, joined_node)`:
  pulls a string from `joined.right`; errors via
  compiler_error if NULL.
- `preprocessor_evaluate_joined_node_defined(compiler, node)`:
  returns `get_definition(preprocessor, name) != NULL`.
- `preprocessor_evaluate_joined_node(compiler, node)`:
  short-circuits to 0 unless `joined.left` is a KEYWORD; for
  `defined` dispatches to the defined-arm; anything else
  returns 0.
- `preprocessor_evaluate` switch gains
  `PREPROCESSOR_JOINED_NODE` routing to
  `evaluate_joined_node`.
- Forward decls at the top of the file now also include
  `preprocessor_evaluate` and `preprocessor_get_definition`
  so the new evaluators (and ch221b's evaluate_unary +
  ch223's evaluate_parentheses, all of which land in the same
  cluster near the top) can call them before their
  definitions later in the file.

Test: `tests/154-preprocessor-defined.sh` confirms `#define
FOO 1; #if defined FOO body #endif` includes the body and
`#if defined BAR body #endif` (BAR undefined) skips it.
