# Feature testing

How to verify that every feature SamCompiler implements still
works end to end after a build. The chapter test suite proves
"the unit didn't regress"; this guide proves "the whole pipeline
still produces correct binaries for non-trivial programs."

## Toolchain

By default the compiler emits 32-bit x86 NASM source and invokes
`nasm` plus `i686-linux-gnu-gcc -static` to produce ELF objects and
linked binaries. You need:

```
sudo apt install nasm gcc-i686-linux-gnu
```

Do not install `gcc-multilib` for `gcc -m32`: on Ubuntu 24.04 it
removes the aarch64 and riscv64 cross compilers
. The driver still falls back to `gcc -m32` if
the i686 cross gcc is missing.

The other targets (`-target=x86_64|aarch64|riscv64`) need their
own tools; see `docs/guides/targets/`.

## Build the compiler

```
cd ~/projects/samcompiler
./build.sh
```

Produces `./main` plus per-translation-unit `.o` files under
`build/`. See `RUNNING_TESTS.md` for the suite + per-test runner.

## The CLI

```
./main <input.c> <output> [object]
```

- `input.c` - source file
- `output` - output path. With no third arg, `./main` writes the
  `.asm`, runs `nasm` and links with `i686-linux-gnu-gcc -static`
  (`gcc -m32` only when the i686 cross gcc is missing), and puts
  the linked binary at `output`. With the literal third arg `object`, the link step is
  skipped (handy when you only want to inspect the `.asm`).

## Verify a sample

```
./main samples/01_arithmetic_control_flow.c samples/01.bin
./samples/01.bin
echo "exit=$?"
```

Compare `$?` against the `EXPECTED EXIT` comment in the source.
Sample 01 should print `exit=55`.

## Feature matrix

| Feature | Sample | Quick check |
|---|---|---|
| Integer arithmetic, for / while / += | `01_arithmetic_control_flow.c` | exit == 55 |
| Preprocessor: `#define`, function macros, `#ifdef`, `##`, `#x` | `02_preprocessor_kitchen_sink.c` | exit == 42 |
| Preprocessor: `#include` + varargs native dispatch | `03_varargs_sum.c` | exit == 0 (verify asm tags) |
| Types: struct + `sizeof` | `04_struct_offsetof.c` | exit == 8 |
| Types: typedef, pointer indirection, address-of | `05_typedef_pointers.c` | exit == 99 |
| Control flow: switch, case, break, do-while | `06_switch_and_break.c` | exit == 29 |
| Preprocessor: user `#include` paths | `07_include_user_header.c` | exit == 4 |
| Arrays: global struct array, indexed read + write | `08_array_indexing.c` | exit == 25 |
| Recursion, function calls, if, return | `09_factorial_recursion.c` | exit == 120 |
| Struct array + nested loops + swap | `10_bubble_sort.c` | exit == 1 |
| Array-backed linked list, struct member traversal | `11_struct_linked_list.c` | exit == 60 |
| Bitwise &, >>, hex literals, popcount loop | `12_bit_manipulation.c` | exit == 7 |
| char* literal, s[i] read, hash + mod | `13_string_hash.c` | exit == 13 |
| Global struct, char* indexing, switch, word counter | `14_token_scanner.c` | exit == 4 |

## Compile every sample (object mode)

The asm-only / object-mode pipeline only needs `nasm`. Run:

```bash
for src in samples/*.c; do
    base=$(basename "$src" .c)
    out="samples/${base}.asm"
    res=$(./main "$src" "$out" object 2>&1)
    case "$res" in
        *"everything compiled fine"*) echo "$base ... compiled" ;;
        *) echo "$base ... FAIL"; echo "$res" | tail -3 ;;
    esac
done
```

All eight samples should report `... compiled`. This proves the
lexer + preprocessor + parser + validator + codegen + nasm
pipeline works end-to-end for every feature listed in the matrix.

## Run every sample as a binary

This step requires a linker for the target (for x86,
`gcc-i686-linux-gnu`). Use the runner shipped under `samples/`
(add `ARCH=x86_64`, `ARCH=aarch64`, `ARCH=riscv64` or `ARCH=all`
for the other targets):

```bash
bash samples/run_all.sh           # from repo root
bash run_all.sh                   # from inside samples/
```

It rebuilds `./main` if missing, walks `samples/*.c`, compiles
each, runs the produced binary, tabulates exit-code vs
expected, and prints a summary line. Non-zero exit on any
mismatch.

Expected output:

```
01_arithmetic_control_flow ... exit=55 expected=55
02_preprocessor_kitchen_sink ... exit=42 expected=42
03_varargs_sum ... exit=0 expected=0
04_struct_offsetof ... exit=8 expected=8
05_typedef_pointers ... exit=99 expected=99
06_switch_and_break ... exit=29 expected=29
07_include_user_header ... exit=4 expected=4
08_array_indexing ... exit=25 expected=25
09_factorial_recursion ... exit=120 expected=120
10_bubble_sort ... exit=1 expected=1
11_struct_linked_list ... exit=60 expected=60
12_bit_manipulation ... exit=7 expected=7
13_string_hash ... exit=13 expected=13
14_token_scanner ... exit=4 expected=4
```

Any mismatch is a regression; cross-reference against the chapter
notes under `docs/NN-*.md` for the feature in question, and see
`samples/README.md` "Known codegen quirks" for patterns the
compiler handles but with sharp edges.

## Inspect the emitted .asm

When a sample compiles but exits with the wrong code, the next
step is reading the asm. Pass `object` as the third argument so
the linker step is skipped:

```
./main samples/01_arithmetic_control_flow.c samples/01.asm object
less samples/01.asm
```

The `.asm` is human-readable NASM. Things to look for:

- `push dword <N>` lines confirm constants land where you expect
- Per-function `<name>:` labels confirm symbol resolution
- `; NATIVE FUNCTION <name>` tags confirm a native preprocessor
  callback (va_start, va_arg, va_end) dispatched
- `cmp eax, 0` + `sete al` is the canonical "logical not"
  sequence (ch222)
- Per-call `function_call_<id>: dd 0` slots in the `.data`
  section confirm the ch187 indirect-call fix

## Verifying preprocessor expansions only

For sample 02 (preprocessor kitchen sink) you may want to confirm
the right tokens reach the parser. Two routes:

1. Inspect the `.asm`: the constants surviving expansion should
   match what you expect (e.g. `42` rather than `ANSWER`).
2. Hand-build a probe (like the chapter tests do): create a
   `compile_process`, drive lex + `preprocessor_run`, walk the
   resulting `cp->token_vec`. See any of the
   `tests/13x-preprocessor-*.sh` tests for the pattern.

## Adding new samples

Number the file (`09_*.c`, `10_*.c`, ...) and lead with an
`// EXPECTED EXIT: N` comment plus a short feature list. Update
`samples/README.md` and the feature matrix above. Keep each
sample under ~50 lines so it's easy to read top-to-bottom.

When you find a feature that the existing samples don't reach,
that's worth a new sample; it doubles as documentation.

## When the binary won't run

| Symptom | Cause | Fix |
|---|---|---|
| `cannot find -lgcc` | `gcc -m32` used without a 32-bit libc | `sudo apt install gcc-i686-linux-gnu` |
| `Exec format error` | Tried to run an x86 binary in a non-x86 env | Use the WSL distro the project lives in |
| `Issue assemblign...` | Bad asm or nasm not installed | `which nasm`, then read the asm to find the offending line |
| Compile failed silently | Likely a preprocessor parse error | Re-run with object-only and read the asm; the error is usually printed before `./main` exits |

## See also

- `USING_THE_COMPILER.md` for how `./main` works internally
  (the lex / preproc / validate / parse / codegen pipeline,
  the post-codegen assembler and linker shell-out, and how to
  drive the CLI against your own programs)
- `RUNNING_TESTS.md` for the chapter test suite
- `DEBUGGING.md` for failure-shape patterns we've already
  encountered and resolved
