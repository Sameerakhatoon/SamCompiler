# ch71 - implementing the function structures

ch70 is lecture-only. ch71 adds the function payload to `struct node`.

What landed:

- `struct function` in the node union, carrying:
  - `flags` (e.g. `FUNCTION_NODE_FLAG_IS_NATIVE`).
  - `rtype` (return type, a `struct datatype`).
  - `name`.
  - `args { vector, stack_addition }` - vector of parameter
    NODE_TYPE_VARIABLEs plus the byte offset to the first arg above
    EBP.
  - `body_n` - NULL for a function prototype.
  - `stack_size` - total locals.
- `FUNCTION_NODE_FLAG_IS_NATIVE` enum.

No parsing logic yet - ch72 writes the function parser.
