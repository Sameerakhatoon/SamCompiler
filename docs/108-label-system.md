# ch108 - building the label system

Codegen gains entry / exit label bookkeeping for `break` / `continue`
and friends. Real loop emission lands later; ch108 adds the
machinery and a smoke-test invocation at the end of `codegen`.

What landed in `compiler.h`:
- `struct codegen_entry_point { int id; }` and `codegen_exit_point`.
- `struct code_generator { vector* entry_points; vector* exit_points; }`.
- `compile_process.generator` (forward-declared above to avoid
  reordering existing decls).
- `codegenerator_new(process)` forward decl.

What landed in `cprocess.c`: `compile_process_create` allocates the
generator alongside the symbol resolver.

What landed in `codegen.c`:
- `codegenerator_new`: allocates the struct and the two vectors.
- `codegen_label_count`: static counter -> 1, 2, 3, ...
- Entry-point stack: `codegen_register_entry_point`,
  `codegen_current_entry_point`, `codegen_begin_entry_point` (emits
  `.entry_point_N:` and pushes), `codegen_end_entry_point` (pops),
  `codegen_goto_entry_point` (emits `jmp .entry_point_N`).
- Exit-point stack: symmetric, but `_begin_exit_point` does NOT emit
  the label - it's emitted at `_end_exit_point`, which is how break
  jumps work (you know the destination only once the loop body is
  done).
- `codegen_begin_entry_exit_point` / `codegen_end_entry_exit_point`:
  convenience pair for loops.
- `codegen()`: after `.rodata`, opens an entry/exit pair, emits a
  `jmp .exit_point`, a `jmp .entry_point`, closes the pair. Purely
  to exercise the wiring.

Test: `tests/61-label-system.sh` runs `compile_file` and checks that
all four expected labels / jumps appear in the asm.
