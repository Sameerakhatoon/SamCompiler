# ch25 - writing our parser template

`parser.c` is born; the rest of Module 1 fills it in.

What landed:

- `enum { PARSE_ALL_OK, PARSE_GENERAL_ERROR }` in `compiler.h`.
- `int parse(struct compile_process*);` prototype in `compiler.h`.
- `compile_process` got two new fields:
  - `node_vec` - every node the parser allocates (a scratch pool).
  - `node_tree_vec` - just the top-level AST roots.
  Both allocated in `compile_process_create`.
- `parser.c` skeleton:
  - `current_process` static for the running parse.
  - `parse_next()` stub returning -1 so the parser loop exits
    immediately. (Real dispatch lands in ch26+; if it returned 0 today
    we'd infinite-loop pushing NULL.)
  - `parse()` sets the token vector's peek pointer to 0 and runs the
    loop.
- `compile_file` calls `parse(process)` after `lex()`.
- Makefile gets a `parser.o` rule.

Smoke test (`tests/21-parse-stub.sh`) confirms ./main still succeeds
after the parser stub is wired in, and that a fresh compile_process
has both vectors allocated and empty.
