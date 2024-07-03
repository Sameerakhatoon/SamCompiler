# ch97 - integrating fixup into the parser

Wires the ch96 fixup system into the parser so a variable that
references a not-yet-declared struct can be patched once the struct
appears.

What landed in `parser.c`:
- New file-scope `parser_fixup_sys` initialized in `parse()`.
- `struct datatype_struct_node_fix_private { struct node* node; }` and
  `datatype_struct_node_fix` / `datatype_struct_node_end` callbacks.
- `make_variable_node`: after pushing the variable node, if its
  datatype is STRUCT but `struct_node` is NULL, register a fixup with
  a private pointer back to the variable. The fix callback looks the
  struct up by name and patches `type`, `size`, and `struct_node`.
- End of `parse()`: `assert(fixups_resolve(parser_fixup_sys))` so any
  unresolved forward struct ref aborts the parse.

Test: `tests/53-fixup-forward-struct.sh` parses
`struct foo* p; struct foo { int a; int b; };` and confirms `p`'s
datatype.struct_node is non-NULL after parsing.
