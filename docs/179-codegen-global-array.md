# ch179 - generating array variables in the global scope

Global arrays land in .data. `int xs[4];` emits
`xs: times 16 db 0` (16 = 4 * sizeof int).

What landed in `codegen.c`:
- `codegen_generate_variable_for_array(node)`: bail on initializers
  (book's "We don't support values for arrays yet" message), then
  emit `<name>: <kw> 0` where `<kw>` is produced by
  `asm_keyword_for_size(variable_size(node), ...)` -
  variable_size already factors in array dimensions, so this
  produces the `times N db ` form for any array size.
- `codegen_generate_global_variable` checks
  `DATATYPE_FLAG_IS_ARRAY` before the type switch; if set, emits
  via the array path and registers a scope entity so the resolver
  can later resolve `xs[i]` accesses.

Test: `tests/118-codegen-global-array.sh` compiles `int xs[4];`
and confirms the asm has `xs:` and `times 16 db`.
