# ch128 - creating the resolver - Part 9

Array follow path, function-call follow path, and an argument-list
walker.

What landed in `helper.c`:
- `is_argument_operator(op)` / `is_argument_node(node)` for `,`.

What landed in `node.c`:
- `node_valid(node)`: NULL-safe; rejects `NODE_TYPE_BLANK` sentinels.

What landed in `resolver.c`:
- `resolver_follow_array(resolver, node, result)`: walk left
  (`a[i]`'s `a`), then walk right (`i`). Returns the left entity.
- `resolver_get_datatype(resolver, node)`: runs the full follow path
  and returns the last entity's dtype, or NULL on failure.
- `resolver_build_function_call_arguments(...)`: recursively descend
  the call's right-hand argument expression. Comma -> split.
  EXPRESSION_PARENTHESES -> unwrap. Valid leaf -> push onto
  `func_call_data.arguments` and add the aligned slot size (>=
  DATA_SIZE_DWORD) to `*total_size_out`.
- `resolver_follow_function_call(resolver, result, node)`: walk the
  callee, build the FUNCTION_CALL entity with both NO_MERGE flags,
  feed the arguments through the walker, push.
- `resolver_follow_parentheses(resolver, node, result)`: if the
  inside is an IDENTIFIER, treat as a call; otherwise run the
  parens' inner expression through `resolver_follow_exp`.
- `resolver_follow_exp` extended: dispatch to array / parens / access.

Test: `tests/76-resolver-arg-helpers.sh` checks the comma predicate
recognizes `,` only, and `node_valid` rejects NULL + BLANK.
