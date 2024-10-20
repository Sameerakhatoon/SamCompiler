# ch167 - creating switch statements Part 4

`case N:` and `default:` get real codegen labels inside the
switch body, matching the jump targets ch166's jump table emits.

What landed in `codegen.c`:
- `codegen_generate_switch_case_stmt(node)`: asserts the case
  expression is a NUMBER literal, calls
  `codegen_begin_case_statement(value)` to emit
  `.switch_stmt_<id>_case_<index>:`, then emits a
  `; CASE <index>` comment marker and the (currently empty)
  `codegen_end_case_statement`.
- `codegen_generate_statement` dispatches
  `NODE_TYPE_STATEMENT_CASE` and `NODE_TYPE_STATEMENT_DEFAULT`
  (the latter into `codegen_generate_switch_default_stmt` which
  was shipped in ch166).

The book also lands `make_default_node` + `parse_default` here;
both already exist in our parser since the original switch /
case / default parser landed earlier (ch85-89). The book's
parse_case `node_peek()` change (so the case node stays on the
stack) is also already in our tree as G03.

Test: `tests/109-codegen-switch-case.sh` compiles
`int main() { int x = 1; switch(x) { case 1: x = 90; break;
case 2: x = 100; break; } return x; }` and asserts
`; CASE 1`, `; CASE 2`, `_case_1:`, and `_case_2:` all appear.
