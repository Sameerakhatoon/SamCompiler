# ch53 - implementing alignment and padding functions

Four helpers in `helper.c`, plus a `padding` field on `struct var`:

- `padding(val, to)` - bytes needed to make `val` a multiple of `to`.
  Returns 0 for to<=0 or for already-aligned val.
- `align_value(val, to)` - round `val` up to the next multiple of `to`.
- `align_value_treat_positive(val, to)` - same but for negative
  values, aligns toward negative-infinity (used by stack offsets).
- `compute_sum_padding(vec)` - sums the `.var.padding` field across
  every NODE_TYPE_VARIABLE in a statements vector.

`struct var` gains an `int padding` so individual variables can carry
their pre-pad byte count for codegen.

Smoke test (`tests/41-padding-align.sh`) checks the math:
padding(5,4)==3, padding(8,4)==0, align_value(5,4)==8,
align_value(8,4)==8, align_value(15,8)==16.
