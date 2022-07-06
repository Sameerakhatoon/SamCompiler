# ch46 - implementing the calculation of array sizes

ch45 is lecture-only. ch46 fills in the array sizing stub from ch44.

`array_brackets_calculate_size_from_index(dtype, brackets, index)`
now:
- Starts with `size = dtype->size` (the element's own byte count).
- If `index` is past the last bracket, returns `size` as-is (we're
  past all the dimensions; one element).
- Otherwise sets the peek pointer to `index` and iterates through the
  remaining bracket nodes, multiplying their inner NUMBER values into
  the running total.

For `int x[4][3]` with `dtype->size == 4`, starting at index 0:
`4 * 4 * 3 = 48` bytes.

Smoke test (`tests/38-array-size.sh`) feeds `int x[4][3];` and
asserts `nd->var.type.array.size == 48`.
