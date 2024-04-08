# ch26 - creating our node file

`node.c` holds the node-stack helpers the parser will use to build the
AST. State is two `struct vector*`s, both owned by the compile_process:

- `node_vector` - the parser's scratch / work stack.
- `node_vector_root` - just the top-level AST roots.

API:

- `node_set_vector(vec, root_vec)` - point the helpers at a parser's
  vectors. Called once at the top of `parse()`.
- `node_push(node)` - push onto the scratch stack.
- `node_peek_or_null()` - peek (NULL on empty).
- `node_peek()` - peek (asserts non-empty).
- `node_pop()` - pop the scratch stack. If the popped node also sits
  at the top of `node_vector_root`, pop it from there too.

The last bullet is the subtle one: when the parser completes a
top-level construct, it ends up at the top of *both* vectors; popping
from the scratch alone would desync them. `node_pop` keeps them aligned.

Smoke test (`tests/22-node-stack.sh`) builds a probe that pushes two
nodes, peeks, pushes one of them into the root vector, pops, then
checks both vectors and the peek behavior.

Also: `tests/lib.sh` now exports `$LINK_OBJS` so every test probe can
link against the full compiler library without hand-listing files.
That'll matter as more modules land.
