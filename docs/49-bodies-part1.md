# ch49 - implementing bodies (part 1)

Bodies are the parser's representation of statement sequences:
function bodies, struct member lists, control-flow branches. ch49
introduces the single-statement (no braces) form; ch54 adds the
brace-delimited form.

What landed:

- `struct body` in `struct node`: statements vector, total size,
  `padded` flag, largest var node. Used by codegen later for stack
  layout.
- `make_body_node(vec, size, padded, largest)` in `node.c`.
- `parser_current_body` global in `node.c`; threaded into each new
  body via `binded.owner`.
- `parse_statement(history)` - one statement: keyword-led
  (declarations / control-flow) or expression-statement + `;`.
- `parse_symbol()` placeholder for things like `goto labels:`.
- `parse_body_single_statement` - build empty body, set as
  current_body, parse one statement, push it into the body's vector,
  restore parent.
- `parse_body(variable_size, history)` - top-level entry; brace path
  is still a TODO (ch54).
- `parser_append_size_for_node` / `parser_finalize_body` - stubs the
  real size / alignment work attaches to in ch50-53.

No behavioural test yet - the parser still doesn't call `parse_body`
from anywhere user-visible until ch54+ wires it into structures /
functions / control-flow.
