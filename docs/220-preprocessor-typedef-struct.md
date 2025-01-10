# ch220 - finishing the typedef directive - part 2

Adds support for struct / union typedefs. After this,
`typedef struct Point { int x; } P;` registers `P` as a TYPEDEF
whose value is `struct Point`, and the structure body itself is
emitted into the compiler's token_vec so later passes still see
the declaration.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_handle_typedef_body_for_brackets`: recursive
  `{ ... }` walker that pushes every token it reads into
  token_vec, recursing on nested `{` and breaking after pushing
  the matching `}`.
- `preprocessor_handle_typedef_body_for_struct_or_union`:
  consumes `struct`, asserts it; sets td type to
  STRUCTURE_TYPEDEF; pushes `struct` to token_vec; reads the
  next token. If IDENTIFIER, that's the struct name (saved on
  td); peek the following token - if also IDENTIFIER, this is
  a declaration-only typedef (`typedef struct Point point;`)
  and we push + return. Otherwise the while loop handles `{`
  body via for_brackets, or `;` to terminate; any other token
  falls through `push_src_resolve_definition`.
- `preprocessor_handle_typedef_body` now routes `struct` /
  `union` to the new struct/union handler (replacing the prior
  `#warning "dont forget about typedef structs"` stub).
- `preprocessor_token_push_semicolon`: stamps a `;` SYMBOL
  token onto compiler->token_vec.
- `preprocessor_handle_typedef_token`'s STRUCTURE_TYPEDEF
  branch now pushes the captured body tokens through to
  compiler->token_vec via `token_vec_push_src`, appends a
  synthetic `;`, then replaces token_vec with a fresh
  `keyword:struct + identifier:Name` pair which becomes the
  TYPEDEF definition's value. So later uses of the typedef
  name expand back to `struct Name`.

Deviation from upstream: in
`handle_typedef_body_for_struct_or_union`, the upstream code
peeks the leading `{` of the struct body but never consumes it
- when `for_brackets` then calls `next_token`, it re-fetches
`{` and pushes a second copy before walking the real body.
This shifts every subsequent token in `token_vec` by one and
makes `vector_back_or_null` (used for the typedef name) point
at the wrong token. We consume the `{` explicitly before
calling `for_brackets`.

Test: `tests/148-preprocessor-typedef-struct.sh` feeds
`typedef struct Point { int x; } P; P p;` and confirms:
- one TYPEDEF definition named P registers,
- token_vec emits `struct Point { int x ; }` + synthetic `;`
  (struct kw + Point ident + braces all present),
- the second statement expands `P` back to `struct Point` so
  the second occurrence of `struct` and `Point` appear and no
  `P` identifier survives.
