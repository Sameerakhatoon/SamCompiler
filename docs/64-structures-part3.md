# ch64 - implementing structures (part 3)

The struct body parser stops walking-and-discarding and builds a real
`NODE_TYPE_STRUCT` with body + optional attached variable.

What landed:

- `NODE_FLAG_IS_FORWARD_DECLARATION` and `NODE_FLAG_HAS_VARIABLE_COMBINED`
  added to the node flags enum.
- `make_struct_node(name, body)` in `node.c` - flags
  IS_FORWARD_DECLARATION when body is NULL.
- `parse_struct_no_new_scope(dtype, is_forward_declaration)` rewritten:
  1. If not forward decl, call `parse_body` with
     `HISTORY_FLAG_INSIDE_STRUCTURE`. Pop the body.
  2. `make_struct_node(type_str, body)`, pop.
  3. Stamp `dtype->size = body->body.size` and
     `dtype->struct_node = struct_node`.
  4. If next is an identifier, that's the attached variable:
     `struct foo { ... } v;`. If the struct was anonymous, the var name
     becomes the type name (and the NO_NAME flag is cleared).
  5. `expect_sym(';')`, push struct_node.
- `parse_variable_function_or_struct_union` now pops the struct, runs
  `symresolver_build_for_node`, and pushes back.
- `symresolver.c::symresolver_build_for_structure_node` registers the
  struct under its name (skips forward decls).
- `parse(process)` now calls `symresolver_initialize` +
  `symresolver_new_table` so the symbol table actually exists. Without
  this, struct registration would dereference NULL - caught and fixed
  here, no separate gotcha because the table just hadn't been set up
  before there were any registrations to do.

Smoke test (`tests/43-struct-with-body.sh`) feeds
`struct abc { int a; int b; };` and asserts NODE_TYPE_STRUCT named
"abc" with body_n containing 2 statements, plus a symresolver lookup
for "abc" succeeds.
