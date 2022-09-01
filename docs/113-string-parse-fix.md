# ch113 - fixing issue parsing strings

The book commit adds a `TOKEN_TYPE_STRING` branch to
`parse_expressionable_single` that calls `parse_single_token_to_node`.

We already shipped that branch as part of an earlier chapter (the
string-token wiring lands at parse-stub time in our chapter ordering),
so ch113 is a no-op for SamCompiler. The behavior is already covered
by `tests/64-global-string-init.sh` (string literal feeds correctly
through expression parsing into the global initializer path).

No code changes; this note exists so the chapter ledger is complete.
