# ch192 - creating the expressionable system - Part 1

First implementation chapter for the preprocessor's generic
expression machinery. The .c body now wires up the callback-driven
parse loop. Only NUMBER tokens land for real - everything else
falls through the switch and returns -1 to stop the loop.

What landed:
- `compiler.h`: `TOKEN_FLAG_IS_CUSTOM_OPERATOR` flag (0b1).
- `expressionable.c`:
  - `expressionable_callbacks()` accessor returns
    `&expressionable->config.callbacks`.
  - Node stack helpers `expressionable_node_push / _pop /
    _peek_or_null` over `node_vec_out`.
  - `expressionable_ignore_nl()` skips a backslash + newline pair
    in the token stream (line continuations).
  - `expressionable_peek_next()` peeks past any line continuations.
  - `expressionable_parse_number()` calls the user's
    `handle_number_callback` and pushes the returned node.
  - `expressionable_parse_token()` switches on token type; only
    `TOKEN_TYPE_NUMBER` is wired in this chapter.
  - `expressionable_parse_single_with_flags()` peeks the next
    token, asks `is_custom_operator` first (sets the
    `TOKEN_FLAG_IS_CUSTOM_OPERATOR` flag when true, then a TODO
    comment stub for parse_exp), otherwise routes to
    `parse_token`. Pops the resulting node, asks
    `expecting_additional_node`, if so recursively parses one
    more and conditionally joins via `should_join_nodes` +
    `join_nodes`. Pushes the final node back.
  - `expressionable_parse()` loops `parse_single` until it
    returns non-zero.

The `#warning "Come back and implement parse_exp"` is verbatim
from the book; later chapters wire up the operator path.

Test: `tests/123-expressionable-system-1.sh` stands up a tiny
expressionable with a single NUMBER token, supplies its own
callbacks, and confirms `expressionable_parse` invokes
`handle_number_callback` exactly once, `is_custom_operator`
exactly once, `expecting_additional_node` exactly once, and
leaves one node on `node_vec_out`. Also verifies
`TOKEN_FLAG_IS_CUSTOM_OPERATOR == 1`.
