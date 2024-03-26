# ch14 - creating a new-line token

`\n` no longer falls through to `handle_whitespace`; it gets its own
`case '\n'` arm in the dispatch switch, which calls
`token_make_newline` to consume the byte and emit a
`TOKEN_TYPE_NEWLINE`.

Why a separate type? The preprocessor (much later) cares deeply about
line boundaries:
- `#define X 42` ends at the newline.
- Backslash-newline is a line-continuation.
- `#error msg` ends at the newline.

The parser will generally skip these tokens, but they need to exist in
the stream so the preprocessor pass can see them.

Smoke test (`tests/12-newline-tokens.sh`) feeds a four-line input and
asserts exactly three NEWLINE tokens come back.
