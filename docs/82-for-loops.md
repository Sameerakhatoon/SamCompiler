# ch82 - implementing for loops

`for (init; cond; loop) body` parses to a NODE_TYPE_STATEMENT_FOR.

What landed:

- `struct for_stmt { init_node, cond_node, loop_node, body_node }`
  added to `struct statement`.
- `make_for_node(init, cond, loop, body)` in `node.c`.
- `parser.c`:
  - `parse_for_loop_part(history)` - parses an expression terminated
    by `;`; returns false (and leaves nothing on the stack) for
    empty parts.
  - `parse_for_loop_part_loop(history)` - parses the trailing loop
    expression terminated by `)`.
  - `parse_for_stmt(history)`:
    1. eat `for(`,
    2. parse init / cond / loop, popping each if present,
    3. eat `)`,
    4. parse body,
    5. `make_for_node(init, cond, loop, body)`.
  - `parse_keyword` grows a `"for"` arm.

No new dedicated test for this one - existing function-body tests
fail-fast on parse errors, and the `for` body is just `parse_body`
under the hood.
