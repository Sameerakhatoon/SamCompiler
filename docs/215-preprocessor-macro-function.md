# ch215 - implementing macro functions

Lands the macro-function execution machinery in
`preprocessor/preprocessor.c`. The actual wire from
`evaluate_exp` -> `evaluate_function_call` is still gated behind
a TODO `#warning` upstream (next chapters complete it); this
chapter ships the helpers so they're ready.

What landed:
- Includes `<assert.h>`.
- `preprocessor_function_arguments_create()`: callocs the
  outer struct and creates the inner `arguments` vector of
  `struct preprocessor_function_argument`.
- `preprocessor_number_push_to_function_arguments`: stamps a
  NUMBER token with `llnum = number` and pushes through
  `preprocessor_token_push_to_function_arguments`.
- `preprocessor_exp_is_macro_function_call(node)`: true when
  the node is an EXPRESSION_NODE with op `()` and its left
  child is an IDENTIFIER_NODE.
- `preprocessor_evaluate_function_call_argument`: recursive
  walk - COMMA-joined expressions split into separate args,
  PARENS-wrapped expressions unwrap, anything else evaluates
  and gets pushed as a number arg.
- `preprocessor_evaluate_function_call_arguments`: thin
  wrapper.
- `preprocessor_is_macro_function`: true for
  MACRO_FUNCTION or NATIVE_CALLBACK.
- `preprocessor_function_arguments_count`: 0 for NULL,
  vector_count otherwise.
- `preprocessor_macro_function_push_argument`: if the named
  argument exists in the definition, push its tokens onto
  value_vec_target; return the argument index or -1.
- `preprocessor_token_vec_push_src_token_to_dst`: trivial
  push.
- `preprocessor_token_vec_push_src_resolve_definition` / `_s`:
  scaffold the path that will eventually expand macros inside
  macro values. Currently just pass-through with TODO
  `#warning`s for typedef / identifier resolution.
- `preprocessor_macro_function_push_something_definition`:
  resolves an identifier token as either a macro-function
  argument or a referenced definition; otherwise -1.
- `preprocessor_macro_function_push_something`: today just
  pushes the token verbatim onto value_vec_target. TODO
  warning for `process concat`.
- `preprocessor_macro_function_execute`: looks up the
  definition, asserts macro-function-ness, checks arg count,
  iterates the definition's value tokens calling
  `macro_function_push_something`, then either evaluates the
  resulting vector (if `PREPROCESSOR_FLAG_EVALUATE_NODE`) or
  pushes it through to `compiler->token_vec`.
- `preprocessor_evaluate_function_call`: extracts macro name +
  call arguments from the EXPRESSION node, runs
  `evaluate_function_call_arguments` to evaluate each arg,
  invokes `macro_function_execute` with
  `PREPROCESSOR_FLAG_EVALUATE_NODE`.
- `preprocessor_evaluate_exp` gains an `if (exp_is_macro_
  function_call(node))` check guarded by a TODO `#warning
  "handle macro function call"` - i.e. the dispatch is
  staged but doesn't return yet, so normal binary handling
  still runs underneath.

Upstream took this chapter as a large whitespace-reformat
pass over the entire file as well. We absorb the reformatted
file wholesale; the ch200 deviation (default arm in
`preprocessor_handle_token` for pass-through) is verbatim in
upstream by now so our content stays consistent.

Test: `tests/143-preprocessor-macro-function.sh` verifies the
new helpers link and behave: `function_arguments_count(NULL)`
returns 0, `function_arguments_create()` gives a zero-sized
arguments vector, and `is_macro_function` correctly
discriminates STANDARD vs MACRO_FUNCTION definitions.
