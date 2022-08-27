# ch106 - generating global variables

`.data` section now emits NASM definitions for each top-level
`int` / `char` (etc.) variable.

What landed in `codegen.c`:
- `asm_keyword_for_size(size, tmp_buf)`: maps byte sizes to
  `db` / `dw` / `dd` / `dq`. Non-primitive sizes fall back to
  `times N db `.
- `codegen_generate_global_variable_for_primitive(node)`: emits
  `<name>: <kw> 0`. Initializer handling is a placeholder until
  ch111 / ch112.
- `codegen_generate_global_variable(node)`: prepends a comment line
  with the type+name, then dispatches by `dtype.type`. Float / double
  raise `compiler_error`.
- `codegen_generate_data_section_part(node)`: switch on `node->type`;
  `NODE_TYPE_VARIABLE` -> the global-variable path; everything else
  ignored.

Test: `tests/60-global-vars-codegen.sh` compiles
`int x; int y; char e;` and grep-asserts `x: dd 0`, `y: dd 0`,
`e: db 0` in the asm output.
