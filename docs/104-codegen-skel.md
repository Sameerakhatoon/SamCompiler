# ch104 - building the codegen fundamentals

Module 2 begins. Just the scaffolding for now.

What landed:
- `codegen.c`:
  - file-static `current_process`.
  - `asm_push_args(ins, va_list)` and `asm_push(ins, ...)` write a
    formatted line to stdout and (if open) to
    `compile_process->ofile`.
  - `codegen(process)` sets `current_process` and emits a placeholder
    `jmp label_name` so the wiring is testable end-to-end.
- `compiler.h`:
  - `CODEGEN_ALL_OK` / `CODEGEN_GENERAL_ERROR` enum.
  - `int codegen(struct compile_process*)` forward decl.
- `compiler.c`: `compile_file` runs `codegen` after `parse` and bails
  out on non-OK.
- `Makefile`: new `codegen.o` target, added to `OBJECTS`.

Test: `tests/58-codegen-stub.sh` calls `compile_file` against a
trivial input, checks `compile_file` returns OK, and grep-matches
`jmp label_name` in both stdout and the on-disk output file.
