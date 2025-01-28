# ch231 - implementing includes - part 3

Adds the static-include fallback so the preprocessor can serve
"built-in" headers (e.g. `stddef-internal.h`,
`stdarg-internal.h`) from C code without touching the
filesystem.

What landed:
- `compiler.h`: decl for
  `preprocessor_static_include_handler_for(filename)`.
- `preprocessor/static-include.c` (new): the handler-lookup
  table. Returns `preprocessor_stddef_include` for
  `stddef-internal.h`, `preprocessor_stdarg_internal_include`
  for `stdarg-internal.h`, NULL otherwise.
- `preprocessor/static-includes/stddef.c` (new):
  `preprocessor_stddef_include` stub with TODO #warning
  (real stddef content lands in a later chapter).
- `preprocessor/static-includes/stdarg.c` (new):
  `preprocessor_stdarg_internal_include` stub with TODO
  #warning ("Create VALIST").
- `Makefile`: build rules for the three new translation
  units; OBJECTS list now includes `static-include.o`,
  `static-includes/stdarg.o`, `static-includes/stddef.o`.
- `build.sh`: `mkdir -p` now also creates
  `build/static-includes` so the per-file rules can drop
  their object files there.
- `tests/lib.sh`: `LINK_OBJS` now also picks up
  `$REPO_ROOT/build/static-includes/*.o`.
- `preprocessor/preprocessor.c`: when
  `compile_include` returns NULL,
  `handle_include_token` now calls
  `preprocessor_static_include_handler_for(path)`; if a
  handler exists, runs `preprocessor_create_static_include`
  (registers the included_file + invokes the handler) and
  returns. Only if no handler exists does the chapter's
  `compiler_error` fire. Replaces the prior
  `#warning "Check for static includes"` stub.

Test: `tests/161-preprocessor-static-include.sh` confirms
`preprocessor_static_include_handler_for` recognizes
`stddef-internal.h` and `stdarg-internal.h` and returns NULL
for an unknown filename.
