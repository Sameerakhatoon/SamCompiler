# ch178 - generating unions

Mirror of the ch149 struct path: a top-level union variable lands
in .data sized to the union's largest member.

What landed in `codegen.c`:
- `codegen_generate_global_variable_for_union(node)`: identical
  shape to the struct version - bail on initializers, emit
  `name: <kw> 0` sized to `variable_size(node)`.
- `codegen_generate_union(node)`: top-level union with an attached
  variable (`union foo { ... } v;`) emits the variable via the
  primitive global path.
- `codegen_generate_global_variable` extended with the
  `DATA_TYPE_UNION` case.
- `codegen_generate_data_section_part` extended with the
  `NODE_TYPE_UNION` case.

Test: `tests/117-codegen-union-global.sh` compiles
`union abc { int x; int y; }; union abc a;` and asserts
`a: dd 0` (the union takes the size of its largest member, 4 bytes).
