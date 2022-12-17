# ch207 - creating the ifdef macro

Adds `#ifdef`. With this, the preprocessor can gate token
pass-through on whether a name has been registered by a prior
`#define`.

What landed in `preprocessor/preprocessor.c`:
- Forward decl for `preprocessor_handle_token` since
  `read_to_end_if` recursively hands tokens back to it.
- `preprocessor_token_is_ifdef`: gate + S_EQ "ifdef".
- `preprocessor_get_definition`: linear scan over
  preprocessor->definitions matching name, returns the
  definition pointer or NULL.
- `preprocessor_hashtag_and_identifier(compiler, str)`:
  no-side-effect peek + match. If the next two tokens look
  like `#` followed by either an identifier == str or a
  keyword token spelled str, consume both and return the
  target; otherwise restore the saved peek state and return
  NULL.
- `preprocessor_is_hashtag_and_any_starting_if`: checks for
  `#if` / `#ifdef` / `#ifndef`. Used by skip_to_endif to
  account for nesting.
- `preprocessor_skip_to_endif`: consumes tokens until
  `#endif`, recursing into nested `#if*` blocks.
- `preprocessor_read_to_end_if(compiler, true_clause)`: if
  true_clause, hand each token to handle_token; otherwise
  skip, and skip_to_endif for any nested `#if*`.
- `preprocessor_handle_ifdef_token`: read the name; error if
  no name; look up via get_definition; call read_to_end_if
  with true_clause = (definition != NULL).
- `preprocessor_handle_hashtag_token` gains an `else if` arm.

Test: `tests/137-preprocessor-ifdef.sh` confirms `#define A 1`
followed by `#ifdef A int x; #endif` pushes 3 tokens into
token_vec, while `#ifdef B int y; #endif` (B undefined) pushes
none.
