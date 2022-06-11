# ch27 - creating our first node

The parser does real work for the first time. It turns the trivial
single-token kinds (NUMBER, IDENTIFIER, STRING) into matching AST
nodes (`NODE_TYPE_NUMBER` / `_IDENTIFIER` / `_STRING`) and pushes them
into `node_tree_vec`.

What landed:

- `node_create(_node)` in `node.c` - the standard "build a node":
  malloc, memcpy the caller's stack struct over, push onto the node
  stack, return the pointer.
- `token.c` got two helpers used by the parser to skip noise:
  `token_is_symbol(t, c)` and
  `token_is_nl_or_comment_or_newline_seperator(t)`. (The "seperator"
  typo is preserved verbatim from the upstream book.)
- `parser.c`:
  - `parser_ignore_nl_or_comment` - peel newline / comment / `\`
    line-continuation tokens off the stream before the parser sees
    them.
  - `token_next` / `token_peek_next` - the only two ways the parser
    moves through the token stream.
  - `parse_single_token_to_node` - the 1:1 cases.
  - `parse_next` dispatch:
    NUMBER/IDENTIFIER/STRING -> single-token node; anything else
    returns -1 to stop the loop (ch28+ widens the dispatch).
  - `parse()` now actually walks the token vector, pushes nodes to
    `node_tree_vec`, and uses `node_peek()` for the loop body.

Smoke test (`tests/23-first-nodes.sh`) feeds `5837 ABCD` and asserts
two top-level nodes: NUMBER 5837 then IDENTIFIER ABCD.
