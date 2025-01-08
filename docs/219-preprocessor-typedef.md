# ch219 - finishing the typedef directive - part 1

Adds the preprocessor's handling of `typedef`. After this, a
top-level `typedef int ABC;` registers a TYPEDEF-style
preprocessor definition whose value is the `int` token vector;
later uses of `ABC` expand back to `int`.

What landed in `preprocessor/preprocessor.c`:
- Forward decls expanded: tightened pointer-spelling on
  `handle_identifier_for_token_vector`, added
  `preprocessor_definition_value`, added
  `token_vec_push_src_resolve_definition`.
- `preprocessor_peek_next_token_with_vector_no_increment` and
  `preprocessor_next_token_with_vector`: priority-vector
  walkers; when the priority vector is exhausted and
  `overflow_use_compiler_tokens` is true, fall back to peeking
  the compiler's original token stream.
- `preprocessor_definition_create_typedef`: callocs a TYPEDEF
  definition, stashes `_typedef.value = value_vec`, pushes
  onto preprocessor->definitions.
- `preprocessor_definition_value_for_typedef(_or_other)`:
  returns `_typedef.value` for TYPEDEF, recurses to the
  standard path for anything else.
- `preprocessor_definition_value_with_arguments` TYPEDEF case
  now returns `value_for_typedef` instead of the prior
  TODO #warning + NULL.
- `preprocessor_token_is_typedef`: keyword gate + S_EQ.
- `preprocessor_handle_typedef_body_for_non_struct_or_union`:
  set type STANDARD, then push tokens through
  `token_vec_push_src_resolve_definition` until a `;` is hit.
- `preprocessor_handle_typedef_body`: peek the first token; if
  it's the `struct` keyword, TODO #warning (struct typedef
  lands later); otherwise route to the non-struct body
  handler.
- `preprocessor_handle_typedef_token`: build a token_vec, run
  the body handler, pop the last token as the typedef name
  (must be IDENTIFIER), then `definition_create_typedef`.
- `token_vec_push_src_resolve_definition` typedef branch:
  dispatches to `handle_typedef_token` (with
  `overflow_use_token_vec = true`).
- `preprocessor_handle_keyword`: top-level dispatch for
  KEYWORD tokens - typedef -> `handle_typedef_token` (with
  `overflow_use_token_vec = false`); anything else pushes
  through.
- `preprocessor_handle_token` switch gains `TOKEN_TYPE_KEYWORD`
  routing to `handle_keyword`.

Test: `tests/147-preprocessor-typedef.sh` feeds `typedef int
ABC; ABC x = 50;` and confirms exactly one TYPEDEF definition
named ABC ends up in the preprocessor, the resulting token_vec
contains 5 tokens for the variable declaration (int x = 50 ;),
and `ABC` was expanded back to `int`.
