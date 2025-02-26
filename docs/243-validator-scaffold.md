# ch243 - building the foundations (validator)

First Module 5 chapter. Lands the validator skeleton between
parse and codegen so later chapters can hang scope / type /
statement checks off this stage.

What landed:
- `compiler.h`:
  - New enum `VALIDATION_ALL_OK / VALIDATION_GENERAL_ERROR`.
  - `int validate(struct compile_process* process)` decl.
- `validator.c` (new):
  - `validate_initialize(process)`: stub.
  - `validate_destruct(process)`: stub.
  - `validate_tree(process)`: returns VALIDATION_ALL_OK.
  - `validate(process)`: init -> tree -> destruct, returns
    tree's result.
- `Makefile`: `./build/validator.o` joins OBJECTS + build rule
  for `./validator.c`.
- `compiler.c`: `compile_file` now calls `validate(process)`
  between `parse(process)` and `codegen(process)`. Result other
  than `VALIDATION_ALL_OK` short-circuits to
  COMPILER_FAILED_WITH_ERRORS.

Test: `tests/170-validator-scaffold.sh` confirms a trivial
source still reaches codegen end-to-end after the validator
insert and that `validate()` returns `VALIDATION_ALL_OK` on a
fresh compile_process.
