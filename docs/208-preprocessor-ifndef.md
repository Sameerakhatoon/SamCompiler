# ch208 - creating the ifndef macro

Adds `#ifndef`. Mirror of `#ifdef` (ch207): same get_definition
lookup, but `read_to_end_if` is called with
`true_clause = (definition == NULL)` so the body is included
when the name is NOT defined.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_token_is_ifndef`: gate + S_EQ "ifndef".
- `preprocessor_handle_ifndef_token`: read name, error if
  missing, look up via get_definition, call read_to_end_if
  with true_clause = (definition == NULL).
- `preprocessor_handle_hashtag_token` gains an `else if` arm.
- Drive-by from upstream: ch207's "No condition token was
  provided." error string gets a trailing `\n` added.

Test: `tests/138-preprocessor-ifndef.sh` confirms `#define A 1
+ #ifndef A body #endif` skips the body and `#ifndef B body
#endif` (B undefined) includes it.
