# ch149 - generating structures (part 1)

Top-level struct definitions with an attached variable now emit
real `.data` storage. The symbol resolver also starts registering
variable + function names, and parser scope transitions are mirrored
on the resolver side so codegen can find what it needs.

What landed in `symresolver.c`:
- `symresolver_build_for_variable_node` / `_function_node` no longer
  error; they register the variable / function name via
  `symresolver_register_symbol`.

What landed in `parser.c`:
- `make_variable_node_and_register` also calls
  `resolver_default_new_scope_entity` so each declared variable
  exists on the resolver-side scope chain too.
- `parse_function` brackets the function body with
  `resolver_default_new_scope` / `_finish_scope`.
- `parse_body` brackets the body's statements with the same pair.
- `parse_struct` / `parse_union` open + close a resolver scope on
  the `{ ... }` path (skipped for forward declarations).
- `parse_keyword_for_global` starts the top-level history with
  `HISTORY_FLAG_IS_GLOBAL_SCOPE` and registers every NODE_TYPE_*
  it returns (VARIABLE / FUNCTION / STRUCT / UNION) with the
  symbol table.

What landed in `resolver.c`:
- `resolver_create_new_entity_for_var_node_custom_scope` stamps
  `entity->var_data.dtype` in addition to `entity->dtype` so the
  read-side resolver code reading the var_data variant sees the
  same type.

What landed in `codegen.c`:
- `codegen_generate_global_variable_for_struct`: zero-init
  storage at the struct's byte size; struct initializers not yet
  supported (errors with a stable message).
- `codegen_generate_global_variable` extended with the
  `DATA_TYPE_STRUCT` case.
- `codegen_generate_struct`: top-level struct with
  `NODE_FLAG_HAS_VARIABLE_COMBINED` emits the attached variable.
- `codegen_generate_data_section_part` extended with the
  `NODE_TYPE_STRUCT` case.

Test: `tests/96-codegen-struct-global.sh` compiles
`struct foo { int a; int b; } v;` and confirms `v: dq 0` (8-byte
slot) lands in `.data`.
