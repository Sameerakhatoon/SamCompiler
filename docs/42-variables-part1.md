# ch42 - implementing variables (part 1)

The parser now actually builds variable nodes. `int x = 50` ->
NODE_TYPE_VARIABLE { name="x", type=int datatype, val=NUMBER(50) }.

Changes:

- `struct datatype` moved earlier in `compiler.h` (above `struct
  node`) because the new `var` payload embeds it by value, not by
  pointer. The duplicate later definition is removed.
- `struct node` gains a `struct var { datatype type; const char* name;
  struct node* val; } var` in the composite union.

`parser.c`:
- `parse_expressionable_root(history)` - parse one expression and
  leave the result on the stack for the caller to pop.
- `make_variable_node(dtype, name_token, value_node)` - builds the
  NODE_TYPE_VARIABLE.
- `make_variable_node_and_register(...)` - calls `make_variable_node`,
  pops the new node, **TODO**: scope-offset + scope_push (ch43-44),
  re-pushes.
- `parse_variable(dtype, name_token, history)` - if next is `=`,
  eat it and parse the RHS expression as the var's value. **TODO**:
  `[N]` array brackets land in ch45.
- `parse_variable_function_or_struct_union` now:
  1. parses datatype + ignore-int (as before),
  2. consumes the identifier token for the variable's name (errors if
     it's not an identifier),
  3. **TODO**: ch46 will check for `(` to switch to function-decl path,
  4. routes to `parse_variable`.
- `parse_keyword_for_global` finally re-enables the
  `node_pop` + `node_push` since real declaration nodes are now
  produced.

Smoke test (`tests/35-variable-decl.sh`) feeds `int x = 50` and
asserts the AST shape: one root of NODE_TYPE_VARIABLE (5), name "x",
.var.type.type == DATA_TYPE_INTEGER (3), .var.val is a NUMBER (2)
with llnum=50.
