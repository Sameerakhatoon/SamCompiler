# ch129 - creating the resolver - Part 10

`NODE_TYPE_BRACKET` follow path + a couple of supporting helpers.

What landed in `array.c`:
- `array_brackets_count(dtype)`: number of declared dimensions.

What landed in `helper.c`:
- `datatype_decrement_pointer(dtype)`: drops `pointer_depth` and
  clears `DATATYPE_FLAG_IS_POINTER` once depth falls to zero or
  below.

What landed in `resolver.c`:
- `resolver_array_bracket_set_flags(bracket_entity, dtype,
   bracket_node, index)`: decide merge flags by case:
  - non-array dtype OR index past the last bracket -> standalone
    pointer arithmetic entity (both NO_MERGE flags plus
    IS_POINTER_ARRAY_ENTITY).
  - non-NUMBER index expression -> standalone but not pointer (both
    NO_MERGE flags).
  - constant in-range index -> JUST_USE_OFFSET so the merge pass
    folds it into the parent.
- `resolver_follow_array_bracket(resolver, node, result)`: bump the
  bracket index when already inside an ARRAY_BRACKET chain, shrink
  the declared array size to the remaining suffix via
  `array_brackets_calculate_size_from_index`, allocate a new entity
  via the user's `new_array_entity` callback, set the flags above,
  and decrement pointer depth for the pointer-array case.
- `resolver_follow_part_return_entity` extended with the BRACKET
  case.

Test: `tests/77-array-brackets-helpers.sh` parses `int x[4][3];`
and confirms `array_brackets_count == 2`, then exercises
`datatype_decrement_pointer` against a depth-2 pointer dtype.
