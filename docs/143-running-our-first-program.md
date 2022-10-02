# ch143 - running our first program

CLI driver wires SamCompiler -> NASM -> gcc -m32, producing a real
runnable 32-bit Linux ELF from a parsed C file.

What landed in `compiler.h`:
- `COMPILE_PROCESS_EXECUTE_NASM` and `COMPILE_PROCESS_EXPORT_AS_OBJECT`
  flags (the second short-circuits the link step).

What landed in `compiler.c`:
- `compile_file` now closes `process->ofile` after codegen so NASM
  can read the freshly-emitted asm.

What landed in `main.c`:
- Accepts `argv[1]` input file, `argv[2]` output file, `argv[3]`
  mode ("exec" default, "object" stops after NASM).
- Always sets `COMPILE_PROCESS_EXECUTE_NASM`; "object" also sets
  `EXPORT_AS_OBJECT`.
- After a successful compile, shells out via `system(...)`:
  - object mode: `nasm -f elf32 <out> -o <out>.o`
  - exec mode:   `nasm -f elf32 <out> -o <out>.o && gcc -m32 <out>.o -o <out>`
- Book typo "assemblign" preserved in the failure message.

Test: `tests/90-end-to-end-nasm.sh` compiles
`int main() { int a = 3 + 4; }` end-to-end via the CLI in `object`
mode and confirms the produced `.o` is a non-empty ELF 32-bit file.
Skips cleanly when `nasm` or `gcc -m32` is missing.
