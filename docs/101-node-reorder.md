# ch101 - additional reordering of nodes

Extra pass on the parser's expression tree to flatten subtrees that
codegen later needs in a specific shape.

What landed in `node.c`:
- `node_is_expression(node, op)`: true iff node is an EXPRESSION with
  exactly that operator.
- `is_array_node(node)`: shorthand for `node_is_expression(node, "[]")`.
- `is_node_assignment(node)`: true for `=`, `+=`, `-=`, `*=`, `/=`.

What landed in `parser.c`:
- `parser_node_move_right_left_to_left(node)`: hoists
  `node->exp.right->exp.left` into a new left child while keeping the
  original op, then pulls the right-grandchild + right's op up to be
  the new right side. Used to "rotate" subscript / assignment /
  `(...)+,` subtrees.
- `parser_reorder_expression`: after the existing priority-flip
  branch, run the rotation when any of these match:
  - left child is an `[]` subscript expression, or
  - right child is an assignment expression, or
  - left is `()` and right is a comma expression.

Test: `tests/57-array-reorder.sh` exercises the three classifier
helpers on synthetic EXPRESSION nodes.
