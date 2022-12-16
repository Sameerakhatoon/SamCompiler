# ch206 - implementing the macro error

Adds `#error`. Mirror of ch205's `#warning`, but routes through
`preprocessor_execute_error` which prepends `#error ` and
calls `compiler_error` - and `compiler_error` exits the process
with `exit(-1)`.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_token_is_error`: gate + S_EQ "error".
- `preprocessor_handle_error_token`: build buffer via
  multi_value_string, hand buffer_ptr to execute_error.
- `preprocessor_handle_hashtag_token` gains an `else if` arm.

Test: `tests/136-preprocessor-error.sh` feeds `#error halt`,
runs the probe, confirms stderr contains `#error halt` and the
process exited before the line after preprocessor_run could
print UNREACHABLE.
