# ch58 - implementing the variable node scope offset (part 1)

The parser now writes a stack offset (`var.aoffset`) onto each
variable declaration. Codegen will read these to lay out function
stack frames.

What landed:

- `struct var` gains `int aoffset` - the aligned offset of the
  variable in its scope.
- `datatype.c::datatype_is_primitive(d)` - convenience (`!is_struct_or_union`).
- `node.c::variable_node(n)` - the "underlying" variable node for a
  VARIABLE / STRUCT / UNION. Lets the offset code work uniformly.
- `node.c::variable_node_is_primitive(n)` - peek through.
- `parser.c::parser_scope_last_entity_stop_global_scope()` - last
  declared entity before the file-scope root.
- `parser.c::parser_scope_offset_for_stack(node, history)` - the
  offset math:
  1. Start with `-variable_size(node)` (local stack grows downward).
  2. If there's a previous entity, add its `aoffset`.
  3. If the new var is primitive, compute its alignment padding via
     the ch53 `padding()` helper and stash it on `var.padding`.
  4. Stamp `var.aoffset = offset`.
- `parser_scope_offset(node, history)` - dispatches; for now always
  the stack path.
- `make_variable_node_and_register` now calls
  `parser_scope_offset` when we're not at the global scope.
- Added `HISTORY_FLAG_IS_UPWARD_STACK` to history flags. The upward
  path is stubbed with a `compiler_error`; ch59+ fills it in.

No new test - existing variable / array tests still pass; ch59-60
will follow with the upward-stack path and pushing to scope.
