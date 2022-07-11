# ch51 - implementing datatype size functions

ch50 is lecture-only on padding/alignment. ch51 adds four size
helpers used by codegen + resolver later:

- `datatype_size(d)` - full size: pointers are 4 (DWORD); arrays use
  `.array.size`; otherwise `.size`.
- `datatype_size_no_ptr(d)` - same, but ignores the IS_POINTER flag.
- `datatype_element_size(d)` - size of one element (pointers are
  DWORD; otherwise `.size`).
- `datatype_size_for_array_access(d)` - sizeof one element when
  indexing through a `struct foo*`: returns the pointed-to size, not
  the pointer size.

Smoke test (`tests/40-datatype-sizes.sh`) builds three datatypes
(plain int, int*, int[4][3]) and asserts the four helpers return the
right values for each.
