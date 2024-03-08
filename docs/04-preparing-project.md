# ch4 - preparing our project

Set up the project skeleton:

- `Makefile` builds `./main` from `main.c` + four object files
  (`compiler.o`, `cprocess.o`, `helpers/buffer.o`, `helpers/vector.o`).
- `compiler.h` declares the top-level API: `compile_file` and
  `compile_process_create`, plus the result enum.
- `compiler.c::compile_file` builds a `struct compile_process` and
  threads it through the (still-unimplemented) lex/parse/codegen stages.
- `cprocess.c::compile_process_create` opens the input file for reading
  and the output file for writing, then calloc's the `compile_process`
  and stashes both `FILE*`s.
- `main.c` calls `compile_file("./test.c", "./test", 0)` and prints the
  result.
- `helpers/buffer.{c,h}` and `helpers/vector.{c,h}` are general-purpose
  containers used everywhere from the lexer onward.

End-to-end checks (`tests/01-builds.sh`, `tests/02-compile-test-c.sh`,
`tests/03-missing-input.sh`) make sure the build is reproducible and
the happy and sad paths both behave.
