# ch112 - implementing string values for global variables

The NODE_TYPE_STRING branch of
`codegen_generate_global_variable_for_primitive` now actually emits.

What landed in `codegen.c`:
- Forward decl for `codegen_register_string` so callers earlier in
  the file can use it.
- String-init branch: `codegen_register_string(sval)` returns the
  string's table label; we emit
  `<varname>: <kw> <label>`. The variable slot holds the address of
  the literal in `.rodata`.

Test: `tests/64-global-string-init.sh` compiles `char* msg = "hi";`
and confirms `msg: dd str_N` plus a matching `str_N: db 'h', 'i', 0`
appears in the asm output.
