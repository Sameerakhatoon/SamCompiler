# ch174 - generating array access

`a[i]` (and `a[N]` with non-constant N) emits a real array-bracket
access: pop base into ebx, evaluate the index, scale by element
size, add, push the address tagged for the next entity load.
Constant-index accesses where the resolver pre-folds the offset
into the base address take a fast path through the existing
variable-access codegen.

What landed in `codegen.c`:
- `codegen_generate_entity_access_for_cast(result, entity)`:
  emits a `; CAST` comment marker. Cast entities don't need a
  runtime op - the dtype on the entity does the work.
- `codegen_generate_entity_access_array_bracket_pointer(result,
   entity)`: pointer-array variant. Pop base, evaluate index, pop
  into eax, `imul eax, datatype_size_for_array_access(dtype)` if
  elements are larger than a byte, `add ebx, eax`, push as
  result_value.
- `codegen_generate_entity_access_array_bracket(result, entity)`:
  routes pointer-array to the helper above; otherwise distinguishes
  JUST_USE_OFFSET (constant index already folded into the entity
  offset -> `add ebx, <offset>`) from runtime
  (`imul eax, <offset>` + `add ebx, eax`).
- Both `_for_entity_for_assignment_left_operand` and
  `_for_entity` (read-side) dispatchers gain
  `ARRAY_BRACKET` + `CAST` cases.

Test: `tests/115-codegen-array-access.sh` compiles
`int main() { int a[4]; int i; int x = a[i]; return x; }` and
asserts an `add ebx, ...` appears after the index resolution
(either the constant-fold form or the imul+add runtime form).
