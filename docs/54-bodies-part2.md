# ch54 - implementing bodies (part 2)

The size-tracking stubs from ch49 turn real. After this chapter,
adding a statement to a body actually updates the body's running
size with proper struct/union handling and alignment.

What landed:

- `node.c::node_is_struct_or_union_variable(node)` - true if the node
  is a NODE_TYPE_VARIABLE whose datatype is struct or union.
- `helper.c::variable_struct_or_union_body_node(node)` - follows a
  struct variable's `.type.struct_node->_struct.body_n`. Union path
  still TODO.
- `parser.c::parser_append_size_for_node` (rewritten):
  - VARIABLE -> if struct/union variable, route to
    `parser_append_size_for_node_struct_union`; else add raw size.
  - VARIABLE_LIST -> iterate and call recursively.
- `parser_append_size_for_node_struct_union` - adds the variable's
  size, then aligns to the largest sub-variable in the struct's body
  (so the next field falls on the right boundary).
- `parser_append_size_for_variable_list` - iterator helper.

Note: upstream's ch54 promises brace-delimited body parsing too. The
core size-tracking lives here; brace-walking lands in subsequent
chapters (the book's "body" subdivision is a little loose).
