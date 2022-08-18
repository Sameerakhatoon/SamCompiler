# ch99 - parsing unions

Mirror of struct parsing for `union`.

What landed:
- `compiler.h`: `struct _union { name, body_n, var }` payload in node
  union; `union_node_for_name` and `make_union_node` forward decls.
- `node.c`:
  - `make_union_node` (mirror of `make_struct_node`).
  - `union_node_for_name` (mirror of `struct_node_for_name`).
  - `variable_node`: return `node->_union.var` instead of asserting.
- `symresolver.c`: `symresolver_build_for_union_node` registers the
  union name as a `SYMBOL_TYPE_NODE`, skipping forward declarations.
- `parser.c`:
  - `size_of_union(name)`: walks the registered union node's body.
  - `parse_union` / `parse_union_no_scope`: clone of the struct flow,
    but the body is parsed under `HISTORY_FLAG_INSIDE_UNION` so the
    body-size code keeps the largest member's size.
  - `parser_datatype_init_type_and_size`: real
    `DATA_TYPE_EXPECT_UNION` branch (replaces the
    "not supported" error).
  - `parse_struct_or_union`: dispatches to `parse_union` for unions.

Test: `tests/55-union-parse.sh` parses
`union foo { int a; char b; };` and checks the node is
NODE_TYPE_UNION, name is "foo", and the recorded size is 4 (largest
member).
