# ch181 - generating variable lists

`int a, b;` declarations now actually get codegen at both global
and local scope - previously only the first variable's slot would
have a defined emitted entry.

What landed in `codegen.c`:
- `codegen_generate_global_variable_list(var_list_node)`: walks
  `var_list.list` and calls `codegen_generate_global_variable` on
  each.
- `codegen_generate_data_section_part` extended with the
  `NODE_TYPE_VARIABLE_LIST` case.
- `codegen_generate_statement` extended with an inline
  `NODE_TYPE_VARIABLE_LIST` case that walks the list and calls
  `codegen_generate_scope_variable` per var.

Test: `tests/119-codegen-var-list.sh` compiles
`int main() { int a, b; a = 50; b = 20; }` and confirms both
`[ebp-4]` and `[ebp-8]` get a stored value.
