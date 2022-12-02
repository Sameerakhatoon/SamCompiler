# ch187 - fixing a bug with function calls within arguments

Calls whose arguments are themselves calls were clobbering the
outer callee (`ecx` holding the target got overwritten while
evaluating each argument). Fix: stash the target in a per-call
slot in `.data` and `call [slot]` instead of `call ecx`.

What landed in `compiler.h`:
- `code_generator.custom_data_section` (vector of `const char*`)
  for lines emitted during codegen and flushed at the end into a
  trailing `.data` section.

What landed in `codegen.c`:
- `codegenerator_new` allocates the new vector.
- `codegen_data_section_add(fmt, ...)`: vsprintfs the formatted
  line into a fresh malloc'd buffer and pushes onto
  `custom_data_section`.
- `codegen_generate_entity_access_for_function_call`:
  - Calls `codegen_label_count()` to mint a fresh id.
  - Queues `function_call_<id>: dd 0` into the .data add-ons.
  - Pops the callee into ebx and stores at
    `[function_call_<id>]` (replacing the old `mov ecx, ebx`).
  - Argument prep then runs without worrying about ecx.
  - The actual call becomes `call [function_call_<id>]`.
- `codegen_generate_data_section_add_ons()`: re-opens `.data`
  and flushes every queued line.
- `codegen()` calls the flush right before `.rodata`.

Tests:
- `tests/95-function-call-codegen.sh` updated - the assertion now
  matches `call [function_call_` instead of `call ecx`.
- `tests/121-call-in-arg-fix.sh` compiles `printf("%i\n",
  special(10))` and confirms two distinct `function_call_N: dd 0`
  slots land in .data.

Module 4 (preprocessor) starts after this fix.
