# ch230 - implementing includes - part 2

Wires `#include`. The directive now reads its next non-newline
token (the path), runs `compile_include` to lex + preprocess
the referenced file using the parent's include_dirs, and
splices the resulting child token_vec into the parent's
token_vec.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_token_is_include`: gate + S_EQ "include".
- `preprocessor_next_token_skip_nl`: consume newlines until a
  real token or NULL.
- `preprocessor_handle_include_token`: read the path token,
  call `compile_include(path, compiler)`, error if NULL
  (with a `#warning "Check for static includes"` TODO for
  the static-include fallback that will land later), splice
  the child's token_vec via `preprocessor_token_vec_push_src`.
- `preprocessor_handle_hashtag_token` gains an `else if` arm.

Test: `tests/160-preprocessor-include.sh` creates a temp
include directory containing `header.h` (which declares
`int included_var;`), prepends that directory onto the
parent's include_dirs via `vector_push_at(0)`, runs lex +
preprocessor over a source that says `#include "header.h"`
followed by `int main() {}`, and confirms both the
`included_var` identifier and the `main` identifier reach
compiler->token_vec.
