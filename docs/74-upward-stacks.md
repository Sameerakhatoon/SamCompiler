# ch74 - dealing with upward stacks

The upward-stack arm of `parser_scope_offset_for_stack` is now anchored
to the function's `stack_addition` (8 bytes by default = saved EBP +
return EIP). Each subsequent argument steps forward by the previous
arg's datatype size.

What landed:

- `node.c::function_node_argument_stack_addition(node)` - read the
  baked-in stack_addition off a NODE_TYPE_FUNCTION.
- `parser.c::parser_scope_offset_for_stack` rewrite of the upward
  branch to anchor on the function's stack_addition and add the
  previous arg's full datatype size on each step.

(My ch73 had a different upward-stack implementation; ch74 brings it
in line with the book's approach.)

No new test - ch73's `tests/46-function-args.sh` covers the path; arg
offsets will become visible to codegen tests later.
