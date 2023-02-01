# ch237 - implementing native functions - part 2

Closes the loop on ch236. Native function symbols registered by
`native_create_function` (e.g. the stub `test()` from
`stdarg-internal.h`) now resolve through the resolver and
dispatch through the codegen v-table at code-emit time.

What landed:
- `compiler.h`:
  - Forward decl for `struct datatype`.
  - `NATIVE_FUNCTION_CALL` typedef drops the unused
    `orinating_function` parameter (book typo preserved on the
    name).
  - `GENERATOR_FUNCTION_RETURN` typedef + `ret` slot on
    `struct generator`.
  - `native_function_get(compiler, name)` decl.
  - New enum tag `RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION` (slotted
    between FUNCTION and STRUCTURE).
  - `struct resolver_native_function { struct symbol* symbol; }`
    added to the `resolver_entity` payload union.
  - `datatype_set_void(struct datatype*)` decl.
- `datatype.c`: `datatype_set_void` body (type=VOID,
  type_str="void", size=0).
- `preprocessor/native.c`:
  - `native_function_get` looks up a NATIVE_FUNCTION symbol via
    `symresolver_get_symbol_for_native_function` and returns
    `sym->data` (or NULL).
- `preprocessor/static-includes/stdarg.c`:
  - `native_test_function` signature drops `orinating_function`
    and now also calls `generator->ret(&dtype, "72")` with a
    4-byte int dtype so the emitted asm pushes a real value.
- `codegen.c`:
  - Forward decl for `asm_push_ins_with_datatype`; v-table
    `.ret = asm_push_ins_with_datatype`.
  - `asm_push_ins_with_datatype(dtype, fmt, ...)` emits `push
    <fmt>` and tags a stackframe element with
    `STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE` carrying the dtype.
  - `codegen_generate_entity_access` early-out: if the root
    entity is `RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION`, look up
    the registered `native_function` via `native_function_get`,
    emit `; NATIVE FUNCTION <name>` and call the registered
    callback with the next entity's
    `func_call_data.arguments`.
- `resolver.c`:
  - `resolver_create_new_entity_for_function_call` now stamps
    `entity->name` from the left operand so native dispatch
    can chase the symbol by name.
  - `resolver_create_new_entity_for_native_function(process,
    name, sym)`: allocates a NATIVE_FUNCTION entity with a void
    dtype + a synthetic function node, anchors at the root
    scope, returns it.
  - `resolver_get_function_in_scope` falls back to
    `RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION` when the regular
    function lookup fails.
  - `resolver_follow_identifier` looks up the name in the
    native-function symbol table when no scope entity is
    found, builds the NATIVE_FUNCTION entity, pushes it onto
    the result.

Test: `tests/167-native-function-dispatch.sh` compiles
`#include <stdarg-internal.h>` + `int main() { test(); }` and
confirms the emitted asm contains both the
`; NATIVE FUNCTION test` dispatch tag and the native
callback's `; TEST FUNCTION ACTIVATED!` payload.
