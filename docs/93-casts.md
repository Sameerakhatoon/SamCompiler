# ch93 - implementing casts

`(T) operand` parses to NODE_TYPE_CAST.

What landed:
- `struct cast { datatype dtype; struct node* operand; } cast` payload.
- `make_cast_node(&dtype, operand)` in node.c.
- `parse_for_cast()` in parser.c: parse datatype, eat `)`, parse the
  operand expressionable, build the cast node.
- `parse_for_parentheses` detects a leading keyword (which can only
  be the start of a type) and dispatches to `parse_for_cast` instead
  of the value/call path.

No new dedicated test - the cast path goes through
parse_for_parentheses which existing tests already exercise.
