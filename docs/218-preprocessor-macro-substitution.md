# ch218 - implementing macro functions part 3

Closes the loop on macro function substitution. After this,
`#define DBL(x) x + x` followed by `DBL(7)` expands to `7 + 7`
in token_vec.

What landed in `preprocessor/preprocessor.c`:
- Forward decl for `preprocessor_handle_identifier_for_token_vector`
  near the top so the resolve helper can call it.
- `preprocessor_token_vec_push_src_resolve_definition` now
  routes IDENTIFIER tokens back through
  `handle_identifier_for_token_vector` so any nested macro
  reference expands. Non-identifiers still push through.
- `preprocessor_macro_function_push_something` now calls
  `push_something_definition` first (which checks if the
  arg_token names a macro-function parameter and pushes the
  cloned call argument tokens through). Only falls back to
  verbatim push when push_something_definition returns -1.
- `preprocessor_evaluate_exp` now actually invokes
  `preprocessor_evaluate_function_call` for macro-function-call
  expressions instead of the previous TODO #warning stub.

Quirk: `preprocessor_definition_argument_exists` walks the
arguments vector by treating each 8-byte slot directly as a
`const char*` and `strcmp`-ing. This works only because the
upstream `vector_push(args, (void*)sval)` bug (ch203) writes
the first `sizeof(char*)` bytes of the arg name into the
slot, and short names (e.g. `x`) end in NUL well within those
8 bytes - so strcmp sees the name correctly. Longer arg names
would still go off the rails. We preserve upstream verbatim.

Test:
- `tests/145-preprocessor-macro-call.sh` (ch217) is updated to
  expect the substituted form `7 + 7` since ch218 makes
  argument substitution actually fire.
- `tests/146-preprocessor-macro-substitution.sh` (new) feeds
  `#define DBL(x) x + x` + `int y = DBL(7);` and confirms
  the post-preprocessor token_vec contains exactly two
  NUMBER(7) tokens.
