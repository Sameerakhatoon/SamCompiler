# ch212 - evaluating the expressions in the preprocessor - part 3

The upstream lecture list jumps from 210 directly to 212; we
follow the same numbering and treat ch212 as the third part of
the evaluator. Adds identifier evaluation so `#if FOO` works
when FOO was registered via `#define`.

What landed in `preprocessor/preprocessor.c`:
- Forward decls at the top for `preprocessor_parse_evaluate`
  and `preprocessor_evaluate` so the helpers below can call
  back recursively.
- `preprocessor_definition_value_for_standard`: returns
  `definition->standard.value`.
- `preprocessor_definition_value_with_arguments`: switches on
  definition type. NATIVE_CALLBACK / TYPEDEF return NULL with
  TODO warnings (upstream typo verbatim: missing closing
  quote). STANDARD falls through to value_for_standard.
- `preprocessor_definition_value`: shorthand for
  with_arguments(definition, NULL).
- `preprocessor_parse_evaluate_token`: pushes a single token
  onto a fresh vector and calls parse_evaluate.
- `preprocessor_definition_evaluated_value_for_standard`: peeks
  the LAST token in the value vector (vector_back). If
  IDENTIFIER, recurses via parse_evaluate_token. If NUMBER,
  returns llnum. Anything else compiler_error.
- `preprocessor_definition_evaluated_value`: switches on
  definition type; STANDARD -> value_for_standard;
  NATIVE_CALLBACK -> TODO #warning + return -1; otherwise
  compiler_error.
- `preprocessor_evaluate_identifier`: look up via
  get_definition. If not found returns true. If
  `vector_count(value) > 1`, builds a fresh expressionable
  with `EXPRESSIONABLE_FLAG_IS_PREPROCESSOR_EXPRESSION`, parses
  the value, recurses into evaluate. If count == 0, returns
  false. Otherwise (exactly 1 token) returns
  definition_evaluated_value.
- `preprocessor_evaluate` switch gains a PREPROCESSOR_IDENTIFIER_NODE
  case routing to evaluate_identifier.

Test: `tests/141-preprocessor-evaluate-ident.sh` confirms
`#define A 7` followed by `#if A body #endif` includes the
body (3 tokens reach token_vec).
