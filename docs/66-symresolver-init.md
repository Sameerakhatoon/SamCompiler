# ch66 - initializing the symbol resolver

Refactor only: move `symresolver_initialize` + `symresolver_new_table`
calls out of `parse()` into `compile_process_create`. The symbol table
now exists from the moment a `compile_process` is born, so anyone
poking at symbols (probes, future passes) doesn't need a separate
init call.

No behaviour change; all 43 tests still pass.
