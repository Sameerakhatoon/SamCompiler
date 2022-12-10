# ch201 - creating the preprocessor expressionable configuration

Builds the bridge from the generic expressionable machinery
(ch192-197) to preprocessor-shaped nodes. Adds 17 callback
shims and a static `preprocessor_expressionable_config` that
the preprocessor will hand to `expressionable_create` whenever
it needs to parse an `#if`-style arithmetic expression.

What landed in `preprocessor/preprocessor.c`:
- New enum value `PREPROCESSOR_PARENTHESES_NODE` slotted
  between EXPRESSION and JOINED. Test 129 updated for the new
  values (jn / tn shift up by one).
- `preprocessor_handle_number_token`: consumes a NUMBER token,
  creates a PREPROCESSOR_NUMBER_NODE carrying its `llnum`.
- `preprocessor_handle_identifier_token`: consumes an
  identifier, calls `preprocessor_is_keyword` to decide
  IDENTIFIER vs KEYWORD tag.
- `preprocessor_make_unary_node` / `make_expression_node` /
  `make_parentheses_node` / `make_tenary_node`: allocate the
  shaped preprocessor_node payload and push it via
  `expressionable_node_push`.
- `preprocessor_get_node_type`: switch maps preprocessor tags
  to `EXPRESSIONABLE_GENERIC_TYPE_*` (NUMBER / IDENTIFIER /
  UNARY / EXPRESSION / PARENTHESES), falling back to NON_GENERIC.
- `get_left_node` / `get_right_node` / `get_left_node_address` /
  `get_right_node_address` / `get_node_operator` /
  `set_expression_node`: trivial accessors over the
  preprocessor_node->exp payload.
- `preprocessor_should_join_nodes` returns true unconditionally
  (next chapter narrows it).
- `preprocessor_join_nodes` builds a PREPROCESSOR_JOINED_NODE
  wrapping previous + current.
- `preprocessor_expecting_additional_node`: returns true when
  the previous node is the keyword "defined" so the next token
  becomes its operand.
- `preprocessor_is_custom_operator`: false for now.
- `preprocessor_expressionable_config` is a static struct
  initializer wiring all 17 callbacks. The preprocessor will
  hand this to `expressionable_create` when it needs to parse
  conditional-expression input.

Test: `tests/131-preprocessor-expressionable-config.sh` creates
an expressionable with the new config, feeds `1 + 2`, and
confirms the resulting top node has tag
`PREPROCESSOR_EXPRESSION_NODE` (4) and the callback maps it to
`EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION`.
