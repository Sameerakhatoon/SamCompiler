# ch68 - error checking our structures

Two robustness fixes:

- `token.c::token_is_identifier(t)` - NULL-safe `t && t->type ==
  TOKEN_TYPE_IDENTIFIER` (mirrors the other `token_is_*` helpers).
- `parse_struct_no_new_scope` now uses `token_is_identifier` instead
  of dereferencing `token_peek_next()->type` directly. Tolerates a
  missing terminator (e.g. `struct foo { ... }` with no following
  declarator and no `;` to follow).

(My `token_next` was already NULL-safe from earlier defensive
patches, so no change there.)

No new test - existing struct tests cover the path; the next chapter
adds a project-cleanup pass.
