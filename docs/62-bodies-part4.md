# ch62 - implementing bodies (part 4)

The brace-delimited body parser is now real.

What landed in `parser.c`:

- `parse_body_multiple_statements(variable_size, body_vec, history)`:
  1. Build a blank body node, set as current, bind owner.
  2. Eat `{`.
  3. Loop until `}`:
     - parse one statement
     - if it's a variable, track the largest by type-size and the
       largest "align-eligible" (primitive) variable
     - push the statement into body_vec
     - call `parser_append_size_for_node(variable_node_or_list(...))`
  4. Eat `}`.
  5. `parser_finalize_body(...)` (ch55) to compute the body size with
     padding + alignment.
  6. Restore parent body; push the finalized body node.
- `parse_body` dispatch:
  - if next token isn't `{`: single-statement (ch49)
  - else: brace path (ch62)

Plus `node.c::variable_node_or_list(node)`: VARIABLE_LIST passes
through; everything else unwraps via `variable_node`.

ch63+ glues the brace body into struct / function parsing so user
code actually exercises it.
