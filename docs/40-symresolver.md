# ch40 - implementing the symbol resolver

ch39 is lecture-only. ch40 introduces `symresolver.c`.

What landed:

- `SYMBOL_TYPE_NODE`, `SYMBOL_TYPE_NATIVE_FUNCTION`,
  `SYMBOL_TYPE_UNKNOWN` enum (compiler.h).
- `struct symbol { name, type, data }` (compiler.h).
- `compile_process.symbols { table, tables }` - active symbol-pointer
  vector + the stack of saved tables.
- `symresolver.c`:
  - `symresolver_initialize` - allocate the `tables` stack.
  - `symresolver_new_table` / `_end_table` - push/pop nested tables
    so locals shadow without permanently clobbering globals.
  - `symresolver_get_symbol(name)` - linear lookup in the active
    table.
  - `symresolver_get_symbol_for_native_function(name)` - same, but
    returns NULL if the type isn't NATIVE_FUNCTION.
  - `symresolver_register_symbol(name, type, data)` - dedup-then-push;
    returns NULL on duplicate name.
  - `symresolver_node(sym)` - data accessor for SYMBOL_TYPE_NODE.
  - `symresolver_build_for_node(process, node)` - dispatch over node
    types: VARIABLE / FUNCTION / STRUCT / UNION. All four call
    `compiler_error("not yet supported")` for now; later chapters
    replace these with real registrations.

Smoke test (`tests/33-symresolver.sh`) drives the full lifecycle:
initialize, new_table, register two distinct symbols, refuse dup,
lookup, push a fresh table and confirm symbols are hidden, end_table
and confirm symbols re-appear.
