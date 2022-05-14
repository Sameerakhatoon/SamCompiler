# SamCompiler

A C compiler built from scratch. Compiles a subset of the C programming
language down to 32-bit Intel assembly, with a preprocessor, parser,
resolver, code generator, and semantic validator.

## Layout

| Path             | What it does                                              |
| ---------------- | --------------------------------------------------------- |
| `main.c`         | Driver: calls `compile_file` on `./test.c`.               |
| `compiler.c/.h`  | Top-level pipeline: lex -> parse -> codegen.              |
| `cprocess.c`     | `compile_process` create/teardown (input/output streams). |
| `helpers/`       | `buffer` (dynamic byte buffer), `vector` (dynamic array). |
| `tests/`         | Bash end-to-end tests, numbered, one per meaningful step. |
| `docs/`          | Chapter notes and gotchas.                                |
| `docs/gotchas/`  | Per-bug write-ups, one file per `Gxx` follow-up commit.   |

## Build

```sh
./build.sh        # clean + make
./main            # runs the compiler on ./test.c
```

## Test

```sh
bash tests/run-all.sh
```
