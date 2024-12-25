# ch216 - getting the value of definitions from within source code

The preprocessor's TOKEN_TYPE_IDENTIFIER path lands. Source-
level uses of a `#define`d name now expand to the definition's
value tokens.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_handle_identifier_for_token_vector(compiler, src, dst, token)`:
  look up `token->sval` in preprocessor->definitions. Not found
  -> push the identifier through to dst as-is (probably a real
  variable name). TYPEDEF -> push the definition value tokens
  via `token_vec_push_src_to_dst`. Macro-function call
  (`peek_no_increment(src) == "("`) -> TODO `#warning "finish
  macro functions first"` and fall through to the standard
  path. Default (STANDARD) -> push value via
  `token_vec_push_src_resolve_definitions`.
- `preprocessor_handle_identifier(compiler, token)`: thin
  wrapper using compiler->token_vec_original and ->token_vec.
- `preprocessor_handle_token` switch gains TOKEN_TYPE_IDENTIFIER
  routing to handle_identifier.

Test: `tests/144-preprocessor-define-expand.sh` feeds `#define
ABC 50` followed by `int x = ABC;` and confirms the resulting
token_vec contains 5 tokens (int / x / = / NUMBER(50) / ;)
with no surviving ABC identifier.
