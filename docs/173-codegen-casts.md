# ch173 - generating casts

`(T) operand` now emits real code: resolve / walk the operand to
push its value, pop into eax, then movsx/movzx to the target
dtype's width.

What landed in `codegen.c`:
- `codegen_generate_cast(node, history)`:
  - Try `codegen_resolve_node_for_value` first; if not resolved,
    walk the operand directly via
    `codegen_generate_expressionable`.
  - Pop into eax.
  - `codegen_reduce_register("eax", datatype_size(cast.dtype),
     signed)` to narrow.
  - Push back tagged with the cast's dtype.
- `codegen_generate_expressionable` dispatches `NODE_TYPE_CAST`.

The book's `codegen_reduce_register` had a `printf("%s ..., %s",
ins, sub_reg)` call that was missing the `ins` argument; we
already shipped the correct version back in ch145, so no diff
needed there.

Test: `tests/114-codegen-cast.sh` compiles
`int main() { int x = 50; char c = (char) x; return c; }` and
asserts the asm uses the eax/al register pair (movsx/movzx
narrowing).
