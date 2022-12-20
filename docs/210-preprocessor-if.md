# ch210 - implementing the if macro

Adds `#if <expr>`. Currently only NUMBER expressions evaluate
to anything meaningful (ch209 + future parts wire identifiers,
unaries, binary ops). The body is included when the evaluated
value is > 0 (note: not `!= 0` - so `#if -1 ...` skips, which
matches upstream verbatim).

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_token_is_if`: gate + S_EQ "if".
- `preprocessor_handle_if_token`: calls
  `preprocessor_parse_evaluate(compiler, compiler->token_vec_original)`,
  hands the result > 0 to `preprocessor_read_to_end_if`.
- `preprocessor_handle_hashtag_token` gains an `else if` arm
  (positioned before the ifdef arm).

Note (upstream verbatim): handle_if_token feeds the full
remaining `token_vec_original` to parse_evaluate. The
expressionable parser may consume more tokens than the rest of
the `#if` line. Today this works because parse_evaluate only
reads what the expressionable callbacks ask for, and the only
wired callback (NUMBER) consumes a single token. Later
chapters extend evaluate and presumably restrict parsing to
the line.

Test: `tests/140-preprocessor-if.sh` confirms `#if 1` includes
the body (3 tokens reach token_vec) and `#if 0` skips it (0
tokens).
