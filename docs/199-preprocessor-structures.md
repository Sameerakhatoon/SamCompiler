# ch199 - creating the preprocessor structures

First preprocessor chapter. Lands `preprocessor/preprocessor.c`
with the tag-union node layout plus the typedef bookkeeping
struct. No logic yet, just the scaffolding the next chapters
will fill in.

What landed:
- `Makefile` gains `./build/preprocessor.o` in `OBJECTS` and a
  rule that compiles `./preprocessor/preprocessor.c` into it.
- `preprocessor/preprocessor.c` (new):
  - `enum { TYPEDEF_TYPE_STANDARD, TYPEDEF_TYPE_STRUCTURE_TYPEDEF }`.
  - `struct typedef_type` with type, definiton_name (typo
    verbatim), value vector, and a nested `typedef_structure`
    with the struct name.
  - `enum { PREPROCESSOR_FLAG_EVALUATE_NODE = 0b1 }`.
  - `enum` covering NUMBER / IDENTIFIER / KEYWORD / UNARY /
    EXPRESSION / JOINED / TENARY node types.
  - `struct preprocessor_node`: tag-union with
    `preprocessor_const_val` (anonymous union over cval / inum
    / lnum / llnum / ulnum / ullnum) and a payload union over
    exp / unary_node (with nested indirection) / parenthesis /
    joined / tenary. Trailing `const char* sval`.

Note 194 is also unnumbered in the upstream lecture list; 198
is similarly skipped in our chapter march to mirror the book's
own numbering.

Test: `tests/129-preprocessor-structures.sh` confirms the
preprocessor object file is produced, main still links, and the
new enum values match the expected order.
