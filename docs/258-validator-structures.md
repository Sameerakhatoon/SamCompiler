# ch258 - implementing the validation of structures and unions

Extends the validator's top-level switch to catch duplicate
struct / union definitions. Forward decls are exempt; everything
else asserts the tag name is unique before registering the node
as a SYMBOL_TYPE_NODE.

What landed in `validator.c`:
- `validate_structure_node(node)`: if not a forward decl,
  `validate_symbol_unique(name, "struct", node)`; then
  `symresolver_register_symbol(name, SYMBOL_TYPE_NODE, node)`.
- `validate_union_node(node)`: same shape for unions.
- `validate_node` switch gains NODE_TYPE_STRUCT / NODE_TYPE_UNION
  cases routing to the above.

Test: `tests/175-validator-structures.sh` confirms:
- Two `struct foo { ... };` defs produce a "Cannot define
  struct" diagnostic.
- One `struct foo { ... };` + `int main()` compiles.

This closes Module 5 as far as the upstream `nibblebits/
PeachCompiler` repo currently goes - everything past this point
is README updates + a separate licensing commit.
