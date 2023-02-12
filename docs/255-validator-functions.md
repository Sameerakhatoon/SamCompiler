# ch255 - implementing functions (validator)

First real check in the validator. validate_tree now walks the
parse tree; NODE_TYPE_FUNCTION dispatches to a function-specific
validator that catches duplicate definitions.

What landed:
- `compiler.h`: `compiler_node_error(node, msg, ...)` decl.
- `compiler.c`: `compiler_node_error` body - vfprintf the
  message to stderr, append `on line <l>, col <c> in file
  <file>` from the node's position, exit(-1). Same diagnostic
  shape as `compiler_error` but anchored at a parse-tree node.
- `validator.c`:
  - `validate_symbol_unique(name, kind, node)`: bail via
    `compiler_node_error` if `symresolver_get_symbol` finds an
    existing symbol with `name`.
  - `validate_body(body)` / `validate_function_body(node)`:
    scaffold loops over the body's statements vector (per-
    statement validation lands in ch257).
  - `validate_function_argument(node)`: stub (variable
    validation lands in ch256).
  - `validate_function_arguments(args)`: iterates the args
    vector calling validate_function_argument on each.
  - `validate_function_node(node)`: if not a forward decl,
    `validate_symbol_unique(name, "function", node)`. Then
    `symresolver_register_symbol(name, SYMBOL_TYPE_NODE, node)`,
    open scope, validate args + body if present, close scope.
  - `validate_node(node)` switch routing
    NODE_TYPE_FUNCTION to validate_function_node.
  - `validate_tree(process)` now opens a scope, walks
    `validation_next_tree_node()` calling `validate_node` on
    each, closes scope, returns ALL_OK.

Test: `tests/172-validator-functions.sh` confirms:
- Two non-forward definitions of `foo` produce the
  `Cannot define function` diagnostic.
- A single `foo()` definition reaches codegen normally.
