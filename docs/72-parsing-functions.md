# ch72 - parsing functions

The parser now emits `NODE_TYPE_FUNCTION` nodes for `T name(args) { ... }`
and `T name(args);`.

What landed:

- `node.c`:
  - `parser_current_function` global mirroring `parser_current_body`.
  - `make_function_node(ret, name, args, body)` builds the node and
    stamps `args.stack_addition = DATA_SIZE_DDWORD` (return EIP).
  - `node_create` now stamps `binded.owner = parser_current_body` and
    `binded.function = parser_current_function` on every new node.
- `parser.c`:
  - New history flag `HISTORY_FLAG_INSIDE_FUNCTION_BODY`.
  - `parse_function_body(history)` - calls `parse_body` with the new
    flag set.
  - `parse_function(ret_type, name_token, history)`:
    1. Open a new scope.
    2. `make_function_node` and grab the pointer; set as current.
    3. If return type is struct/union, bump `stack_addition` by
       DWORD (hidden first arg for the result pointer).
    4. `expect_op("(")` ... TODO(ch73): args ... `expect_sym(')')`.
    5. If the name matches a native function, set
       FUNCTION_NODE_FLAG_IS_NATIVE.
    6. If next is `{`: parse body, attach `func.body_n`.
       Else: it's a prototype, demand `;`.
    7. Close the scope.
  - `parse_variable_function_or_struct_union` now checks for `(` after
    the name and dispatches to `parse_function`.

Smoke test (`tests/45-function-decl.sh`) feeds `int main() { int x; }`
and asserts a `NODE_TYPE_FUNCTION` named "main" with a 1-statement body.

ch73 wires real argument parsing.
