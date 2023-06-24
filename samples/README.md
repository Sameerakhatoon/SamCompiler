# Sample programs

Fourteen C programs that exercise different parts of the
compiler end to end. Each carries an `// EXPECTED EXIT: N`
comment so you can compare the produced binary's exit code
against the intended value.

All samples pass on all four targets (x86, x86_64, aarch64,
riscv64): 14/14 each. None of them needs a separate LP64 answer;
a program that did would add `// EXPECTED EXIT lp64: N` (see
`tests/golden/inputs/39_pointer_size.c`).

See `docs/guides/FEATURE_TESTING.md` for the full workflow.

| # | File | Features under test | Expected exit |
|---|---|---|---|
| 01 | `01_arithmetic_control_flow.c` | int vars, for, while, += | 55 |
| 02 | `02_preprocessor_kitchen_sink.c` | #define, function macros, #ifdef, ##, #x | 42 |
| 03 | `03_varargs_sum.c` | #include `<stdarg.h>`, va_list, va_start, va_arg | 0 |
| 04 | `04_struct_offsetof.c` | struct, sizeof | 8 |
| 05 | `05_typedef_pointers.c` | typedef, pointer indirection, address-of | 99 |
| 06 | `06_switch_and_break.c` | switch, case, break, do-while | 29 |
| 07 | `07_include_user_header.c` | #include of a header via pc_includes/ | 4 |
| 08 | `08_array_indexing.c` | global struct array, indexed read + write | 25 |
| 09 | `09_factorial_recursion.c` | recursion, function calls, if, return | 120 |
| 10 | `10_bubble_sort.c` | struct array, nested for loops, swap | 1 |
| 11 | `11_struct_linked_list.c` | array-backed linked list, struct member traversal | 60 |
| 12 | `12_bit_manipulation.c` | bitwise &, >>, hex literals, popcount loop | 7 |
| 13 | `13_string_hash.c` | char* literal, s[i] read, hash + mod | 13 |
| 14 | `14_token_scanner.c` | global struct, char* indexing, switch, word counter | 4 |

## Quick run

From the repo root:

```
./main samples/01_arithmetic_control_flow.c samples/01.bin
./samples/01.bin
echo "exit=$?"
```

The exit code should match the `EXPECTED EXIT` line in the source.
The x86 link uses `i686-linux-gnu-gcc -static` (package
`gcc-i686-linux-gnu`); without it, or `gcc -m32`, the `./main`
invocation reports `Issue assemblign...` after emitting the asm
.

For another target add `-target=` first; the cross targets run
under qemu-user:

```
./main -target=aarch64 samples/01_arithmetic_control_flow.c /tmp/01.a64
qemu-aarch64 /tmp/01.a64
echo "exit=$?"
```

Toolchains per target: `docs/guides/targets/`.

## Run them all

A runner ships in this directory:

```
bash samples/run_all.sh                 # x86, from repo root
bash run_all.sh                         # x86, from inside samples/
bash samples/run_all.sh ARCH=riscv64    # one target
bash samples/run_all.sh ARCH=all        # x86, x86_64, aarch64, riscv64
```

It rebuilds `./main` if missing, compiles each sample, runs the
produced binary (natively for x86 and x86_64, under qemu-aarch64 /
qemu-riscv64 for the others), and tabulates exit-code vs expected.
Each target ends with `passed: N   failed: M`; `ARCH=all` adds a
summary. Any failure -> exit code 1.

`ARCH=all` ends with:

```
== summary
x86      passed: 14  failed: 0
x86_64   passed: 14  failed: 0
aarch64  passed: 14  failed: 0
riscv64  passed: 14  failed: 0
```

Expected output for one target on a clean tree:

```
01_arithmetic_control_flow       exit=55  expected=55  OK
02_preprocessor_kitchen_sink     exit=42  expected=42  OK
03_varargs_sum                   exit=0   expected=0   OK
04_struct_offsetof               exit=8   expected=8   OK
05_typedef_pointers              exit=99  expected=99  OK
06_switch_and_break              exit=29  expected=29  OK
07_include_user_header           exit=4   expected=4   OK
08_array_indexing                exit=25  expected=25  OK
09_factorial_recursion           exit=120 expected=120 OK
10_bubble_sort                   exit=1   expected=1   OK
11_struct_linked_list            exit=60  expected=60  OK
12_bit_manipulation              exit=7   expected=7   OK
13_string_hash                   exit=13  expected=13  OK
14_token_scanner                 exit=4   expected=4   OK

passed: 14   failed: 0
```

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
grep -E '; (NATIVE|va_|native)' samples/03.asm
```

Should print all the native callback tags (`; NATIVE FUNCTION
va_start`, `; NATIVE FUNCTION __builtin_va_arg`) plus the
per-callback start/end markers.

## Known codegen quirks

A handful of patterns the compiler handles but with surprises
that some samples deliberately work around:

1. `array[i]` from an `int values[]` read in expression position
   sometimes returns the slot ADDRESS rather than the value at
   that address. Workaround used in samples 08 and 10: wrap the
   ints in a single-field struct (`struct cell { int v; }`) and
   read via `values[i].v` instead, which dereferences correctly.

2. Reading the second operand of a comparison like `array[i].v
   > array[k].v` sometimes drops the load and the right side
   reads as 0. Workaround in sample 10: hoist both sides into
   plain int locals before comparing.

3. Calling a function whose return type is `void` and the
   compiler tries to load the result through `movzx eax,
   (null)`, which nasm rejects. Workaround across samples: any
   function called as a statement (vs assigned to a variable)
   returns `int 0;` and is declared `int`.

4. `va_arg(list, int)` returns a typed value of `void*` (the
   native ret datatype is void with pointer_depth=1), so
   participating in `int + va_arg(...)` triggers
   pointer-arithmetic scaling. Sample 03 demonstrates the
   native callback dispatch and emits both tags without
   summing the args.
