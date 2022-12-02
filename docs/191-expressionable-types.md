# ch191 - creating our expressionable structures

Header-only chapter. Lands the generic expressionable type system
in `compiler.h`; ch192+ wires up the implementation. The
preprocessor will reuse this for `#if` arithmetic without
duplicating the parser's precedence + reorder logic.

What landed in `compiler.h`:
- `EXPRESSIONABLE_GENERIC_TYPE_*` (NUMBER / IDENTIFIER / UNARY /
  PARENTHESES / EXPRESSION / NON_GENERIC).
- `EXPRESSIONABLE_IS_SINGLE` / `EXPRESSIONABLE_IS_PARENTHESES`.
- 16 callback typedefs (handle_number, handle_identifier,
  make_expression / unary / unary_indirection / tenary,
  get_node_type / left / right / address, get_operator,
  set_expression, should_join / join, expecting_additional,
  is_custom_operator).
- `struct expressionable_config` carrying the callbacks.
- `EXPRESSIONABLE_FLAG_IS_PREPROCESSOR_EXPRESSION`.
- `struct expressionable { flags; config; token_vec;
   node_vec_out; }`.

Test: `tests/122-expressionable-types.sh` confirms the enum / flag
/ struct decls all compile cleanly.
