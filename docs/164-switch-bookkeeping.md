# ch164 - creating switch statements (bookkeeping)

Switch-statement bookkeeping infrastructure lands on the code
generator. ch166+ actually wires it into statement dispatch.

What landed in `compiler.h`:
- `struct generator_switch_stmt_entity { int id; }`.
- `struct generator_switch_stmt { current; vector* swtiches; }`
  nested inside `code_generator`. Book typo `swtiches` preserved.

What landed in `codegen.c`:
- `codegenerator_new` allocates the swtiches vector.
- `codegen_begin_switch_statement()`: pushes the outer switch's
  data onto the stack, zeroes `current`, emits
  `.switch_stmt_<id>:`.
- `codegen_end_switch_statement()`: emits `.switch_stmt_<id>_end:`,
  pops the outer switch back.
- `codegen_switch_id()`: returns the current switch id.
- `codegen_begin_case_statement(index)`: emits
  `.switch_stmt_<id>_case_<index>:`.
- `codegen_end_case_statement()`: stub (book ships empty).

Test: `tests/107-codegen-switch-helpers.sh` confirms the
`_switch.swtiches` vector is allocated and empty on a fresh
process.
