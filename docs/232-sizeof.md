# ch232 - implementing sizeof

Adds the `sizeof(<datatype>)` operator at the parser. It is
evaluated at parse time and emitted as a NUMBER node holding
`datatype_size(dtype)`; codegen sees a plain constant.

What landed in `parser.c`:
- `parse_sizeof(history)`: `expect_keyword("sizeof")`,
  `expect_op("(")`, `parse_datatype(&dtype)`, `node_create`
  for a NODE_TYPE_NUMBER with `llnum = datatype_size(&dtype)`,
  `expect_sym(')')`. No history flag plumbing needed since the
  node is a literal.
- `parse_keyword` early-out: when the next token is the
  keyword `sizeof`, dispatch to `parse_sizeof` before the
  modifier / datatype gate.

Note: only the `sizeof(type)` form lands here -
`sizeof expr` style isn't wired yet (later chapter).

Test: `tests/162-sizeof.sh` compiles
`int main() { return sizeof(int); }` and confirms the emitted
asm contains `push dword 4` (32-bit `sizeof(int)`).
