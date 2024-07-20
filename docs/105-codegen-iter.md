# ch105 - beginning the iteration of the AST

`codegen` now walks the AST root vector and emits the three standard
asm section headers, with placeholder per-node hooks ready for later
chapters.

What landed in `codegen.c`:
- `codegen_new_scope` / `codegen_finish_scope`: stubs. The resolver
  (Module 3) is what these will defer to.
- `codegen_node_next`: pops one node off
  `current_process->node_tree_vec`.
- `codegen_generate_data_section_part(node)` and
  `codegen_generate_root_node(node)`: per-node hooks. Bodies are
  intentionally empty until ch106+.
- `codegen_generate_data_section` / `codegen_generate_root` /
  `codegen_generate_rod`: print `section .data` / `.text` / `.rodata`
  then walk the root vector calling the matching per-node hook.
- `codegen`: `scope_create_root`, rewind the root vector, run data
  pass, rewind, run text pass, then rodata.

What landed in `parser.c`:
- `parse()` ends with `scope_free_root(process)` so codegen can
  install its own root scope without colliding.

Test: `tests/59-codegen-sections.sh` runs `compile_file` on a trivial
input and confirms `.data`, `.text`, `.rodata` headers all land in
the output file in order.
