# ch170 - creating strings and calling printf

String literals in expression position (e.g. function-call
arguments) now register in the .rodata string table, mov their
asm label into eax, and push as a typed result_value.

What landed in `helper.c`:
- `datatype_for_string()`: synthetic `const char*` datatype
  (DATA_TYPE_INTEGER + IS_POINTER + IS_LITERAL, pointer_depth=1,
  size=DWORD).

What landed in `codegen.c`:
- `codegen_gen_mov_for_value(reg, value, datatype, flags)`:
  emits `mov <reg>, <value>`.
- `codegen_generate_string(node, history)`: calls
  `codegen_register_string(sval)` to get a `str_N` label, moves
  it into eax, and pushes with `dtype = datatype_for_string()`.
- `codegen_generate_expressionable` extended with the
  `NODE_TYPE_STRING` case.

Test: `tests/111-codegen-string-arg.sh` compiles
`int printf(const char* s); int main() { printf("hello world\n"); }`
and asserts the literal expands in .rodata and the `mov eax,
str_N` + `push eax` argument prep appears.
