# ch236 - implementing native functions - part 1

Lands the infrastructure that lets the preprocessor register
C-implemented "native" functions and have the codegen call them
through a v-table. The actual function-call dispatch (and the
test() builtin going from registered to invoked) lands in
part 2.

What landed:
- `compiler.h`:
  - `struct generator_entity_address` (is_stack / offset /
    address / base_address).
  - Function-pointer typedefs: `ASM_PUSH_PROTOTYPE`,
    `NATIVE_FUNCTION_CALL`, `GENERATOR_GENERATE_EXPRESSION`,
    `GENERATOR_ENTITY_ADDRESS`, `GENERATOR_END_EXPRESSION`.
  - `struct generator` (the v-table) with `compiler` +
    `private`.
  - `struct native_function_callbacks { NATIVE_FUNCTION_CALL
    call; }`.
  - `struct native_function { name; callbacks; }`.
  - `native_create_function(compiler, name, callbacks)` decl.
  - `symresolver_register_symbol(process, sym_name, type,
    data)` decl.
- `codegen.c`:
  - Forward decls for the v-table hooks.
  - `_x86_generator_private` struct holding a `history*`
    "remembered" slot.
  - Global `x86_codegen` instance wired to the four hooks +
    the private struct.
  - `codegen_asm_push` non-static shim that calls the static
    `asm_push_args` (since `asm_push` proper is file-static
    by ch143).
  - `codegen_gen_exp` recurses into
    `codegen_generate_expressionable` with
    `codegen_history_down` over the remembered history.
  - `codegen_end_exp` placeholder.
  - `codegen_entity_address` fills out the address view from
    `codegen_entity_private`.
  - `codegen()` now stamps `x86_codegen.compiler =
    current_process` at entry.
- `preprocessor/native.c`: adds `native_create_function`
  (callocs the struct, copies the callbacks, registers via
  `symresolver_register_symbol` with
  SYMBOL_TYPE_NATIVE_FUNCTION).
- `preprocessor/static-includes/stdarg.c`: stub
  `native_test_function` emits a tagged comment line via
  `generator->asm_push`; `preprocessor_stdarg_internal_include`
  registers it as `test`.

Test: `tests/166-native-function-scaffold.sh` confirms all five
slots of the global `x86_codegen` v-table (asm_push, gen_exp,
end_exp, entity_address, private) are non-NULL.
