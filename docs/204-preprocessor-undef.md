# ch204 - implementing undef

Adds `#undef` to the preprocessor. After this, a name registered
by `#define` can be removed.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_token_is_undef`: gate + S_EQ "undef".
- `preprocessor_handle_undef_token`: reads the next identifier
  via preprocessor_next_token and calls
  preprocessor_definition_remove on compiler->preprocessor.
- `preprocessor_handle_hashtag_token` gains an `else if` arm
  for the undef case.

Test: `tests/134-preprocessor-undef.sh` feeds `#define FOO 1`
followed by `#undef FOO`, runs preprocessor_run, and confirms
`compiler->preprocessor->definitions` is empty afterwards.
