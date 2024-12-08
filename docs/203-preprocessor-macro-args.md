# ch203 - creating macro arguments

`#define` now understands a function-style argument list: when
the `(` directly follows the macro name with no whitespace
separating them, the preprocessor reads identifiers separated
by `,` until `)` and stashes them on the definition's
arguments vector. A non-empty arguments vector promotes the
definition type from STANDARD to MACRO_FUNCTION.

What landed:
- `helpers/vector.c`: `vector_create_no_saves` was missing
  `return vector;`. Fixed.
- `preprocessor/preprocessor.c`:
  - `preprocessor_previous_token`: vector_peek_at(pindex - 1).
  - `preprocessor_next_token_no_increment`: peek without bump.
  - `preprocessor_peek_next_token_skip_nl`: loop past NEWLINE
    tokens. (Re-peeks at the end so the caller can still
    consume.)
  - `preprocessor_is_next_macro_arguments`: vector_save, peek
    previous + current, true when current is `(` and previous
    has no whitespace (so `ABC(x)` matches but `ABC (x)`
    doesn't). Restores the save before returning.
  - `preprocessor_parse_macro_argument_declaration`: skip `(`,
    loop reading IDENTIFIERs separated by `,` until `)`. Errors
    via `compiler_error` on non-identifier or malformed
    sequences.
  - `preprocessor_handle_definition_token` removes its
    placeholder #warning and now calls
    is_next_macro_arguments + parse_macro_argument_declaration
    before reading the value tokens.

Upstream bug (preserved verbatim): the arg parser pushes via
`vector_push(arguments, (void*)next_token->sval)`. This passes
the string address as if it were a pointer to a pointer; the
vector_push memcpy then copies the first `sizeof(char*)` bytes
of the string contents into the slot rather than the address.
So `d->standard.arguments` contains garbage when dereferenced
as `const char**`. A later chapter presumably fixes this; we
keep the upstream call verbatim and note the consequence in
test 133 - it just counts arguments rather than reading them
back.

Test: `tests/133-preprocessor-macro-args.sh` feeds `#define
ABC(x, y) x + y`, runs preprocessor_run, and confirms the
definition has name=ABC, type=MACRO_FUNCTION, args vector
count == 2, value vector count == 3.
