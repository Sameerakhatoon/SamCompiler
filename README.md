# SamCompiler

A C compiler built from scratch. Compiles a subset of the C programming
language with a preprocessor, parser, resolver, semantic validator and
code generator, for four targets: 32-bit x86, x86-64, AArch64 and
RISC-V (rv64).

## Targets

| `-target=` | Output | Runs | Guide |
|---|---|---|---|
| `x86` (default) | NASM, ILP32, cdecl | natively | [docs/guides/targets/x86.md](docs/guides/targets/x86.md) |
| `x86_64` | NASM, LP64, SysV arguments in registers | natively | [docs/guides/targets/x86_64.md](docs/guides/targets/x86_64.md) |
| `aarch64` | GNU as, LP64, AAPCS64 | under `qemu-aarch64` | [docs/guides/targets/aarch64.md](docs/guides/targets/aarch64.md) |
| `riscv64` | GNU as, LP64, RISC-V psABI | under `qemu-riscv64` | [docs/guides/targets/riscv64.md](docs/guides/targets/riscv64.md) |

All 14 samples and the 41 golden feature programs pass on all four.
The x86 output is locked byte for byte by `tests/176-x86-golden.sh`.

## Layout

| Path             | What it does                                              |
| ---------------- | --------------------------------------------------------- |
| `main.c`         | Driver: `-target=`, compile, then assemble and link with the target's commands. |
| `compiler.c/.h`  | Top-level pipeline: lex -> preprocess -> parse -> validate -> codegen. |
| `cprocess.c`     | `compile_process` create/teardown (input/output streams, target). |
| `target.c`       | One descriptor per target: sizes, backend, assemble/link/run commands, predefined macros. |
| `codegen.c`      | Hands the AST to the target's backend.                     |
| `codegen_x86.c`, `codegen_x86_64.c`, `codegen_aarch64.c`, `codegen_riscv64.c` | The four backends (one stack machine, four instruction sets). |
| `helpers/`       | `buffer` (dynamic byte buffer), `vector` (dynamic array). |
| `samples/`       | Example programs with expected exit codes; `run_all.sh ARCH=...`. |
| `tools/compare_gcc.sh` | SamCompiler output next to gcc -O0 and -O2 for one target. |
| `tests/`         | Bash end-to-end tests, numbered, one per meaningful step; `tests/golden/` holds the golden inputs and the locked x86 asm. |
| `docs/`          | User guides.                                              |

## Build

```sh
./build.sh        # clean + make -j
```

## Compile a program

```sh
./main in.c out                       # x86, linked
./main -target=x86_64 in.c out        # native 64-bit binary
./main -target=aarch64 in.c out       # then: qemu-aarch64 out
./main -target=riscv64 in.c out.s object   # stop after the assembler
```

See `docs/guides/USING_THE_COMPILER.md`.

## Test

```sh
bash tests/run-all.sh                 # every test
bash samples/run_all.sh ARCH=all      # the samples on every target
```

See `docs/guides/RUNNING_TESTS.md` for which tests run on which
target.
