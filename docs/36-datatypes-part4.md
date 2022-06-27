# ch36 - implementing datatypes and keywords (part 4)

Small chapter: route top-level keywords through a dedicated
`parse_keyword_for_global` instead of going via the
`parse_expressionable` path. token.c also gets NULL guards on the
`token_is_*` helpers (already in place from earlier chapters).

What landed in `parser.c`:

- `parse_keyword_for_global` - calls `parse_keyword` and (per the
  book) pops the produced node. We comment the pop out for now -
  `parse_keyword`'s current path through `parse_datatype` doesn't
  push any node, so the pop would crash. ch37+ pushes a real
  declaration node and the pop will be re-enabled.
- `parse_next` switch gains a `case TOKEN_TYPE_KEYWORD` arm.
- `parse()`'s main loop now only attaches to `node_tree_vec` if there's
  actually a node on the scratch stack (guards the same bug from the
  other side).

Smoke test (`tests/31-keyword-global.sh`) feeds `unsigned char` and
asserts `compile_file` returns OK.
