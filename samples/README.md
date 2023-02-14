# Sample programs

Eight complex C programs that exercise different parts of the
compiler end to end. Each one carries an `// EXPECTED EXIT: N`
comment at the top so you can compare the produced binary's exit
code against the intended value.

See `docs/guides/FEATURE_TESTING.md` for the full workflow.

| # | File | Features under test | Expected exit |
|---|---|---|---|
| 01 | `01_arithmetic_control_flow.c` | int vars, for, while, += | 55 |
| 02 | `02_preprocessor_kitchen_sink.c` | #define, function macros, #ifdef, ##, #x | 42 |
| 03 | `03_varargs_sum.c` | #include `<stdarg.h>`, va_list, va_start, va_arg, va_end | 60 |
| 04 | `04_struct_offsetof.c` | struct, sizeof | 8 |
| 05 | `05_typedef_pointers.c` | typedef, pointer indirection, & | 99 |
| 06 | `06_switch_and_break.c` | switch, case, default, break, do-while | 29 |
| 07 | `07_include_user_header.c` | #include of a user header | 4 |
| 08 | `08_array_indexing.c` | array decl, indexed write, indexed read | 25 |

## Quick run

From the repo root:

```
./main samples/01_arithmetic_control_flow.c samples/01.bin
./samples/01.bin
echo "exit=$?"
```

The exit code should match the `EXPECTED EXIT` line in the source.
Some samples require the 32-bit gcc toolchain to actually produce
a runnable binary; see FEATURE_TESTING.md for the install + run
sequence.

## Sample 07 note

Sample 07 includes `<values.h>` which lives in
`pc_includes/values.h` (committed alongside the other built-in
headers like `stdarg.h` / `stddef.h`). The preprocessor walks
`compile_process->include_dirs`; the default list puts
`./pc_includes` first so the header resolves automatically when
you run `./main` from the repo root.

## What ends up in the binary

Compile a sample in "object" mode and inspect the `.asm` to
spot-check the codegen for each feature:

```
./main samples/02_preprocessor_kitchen_sink.c samples/02.asm object
grep -E 'push dword|main:' samples/02.asm
```

You should see `push dword 42` and `push dword 21` once each
(ANSWER and DOUBLE's two operands), confirming the preprocessor
collapsed everything before the parser saw it.

```
./main samples/03_varargs_sum.c samples/03.asm object
grep -E '; (NATIVE|va_)' samples/03.asm
```

Should print all three native callback tags (`; NATIVE FUNCTION
va_start`, `; NATIVE FUNCTION __builtin_va_arg`, `; NATIVE
FUNCTION va_end`) plus the per-callback start/end markers.
