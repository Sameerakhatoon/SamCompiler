# ch180 - finishing our codegen_generate_global_variable function

Trailing assertion + scope-entity registration so every global
VAR is visible to the resolver after .data emit.

What landed in `codegen.c`:
- `codegen_generate_global_variable` ends with
  `assert(node->type == NODE_TYPE_VARIABLE)` and
  `codegen_new_scope_entity(node, 0, 0)`.

ch179 already added the array-specific scope registration; ch180
closes the loop for all the other (primitive, struct, union)
globals.

Existing tests cover this change indirectly; we don't add a
dedicated test because every global access now relies on the
scope-entity registration to resolve.
