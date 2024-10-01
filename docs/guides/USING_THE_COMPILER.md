# Using the compiler

What actually happens when you run `./main my.c my.bin`, and how
to drive it against your own programs.

## Quick start

The driver takes gcc-style options:

```
./main -target=x86_64 hello.c -o hello      # compile, assemble, link
./main -target=x86_64 a.c b.c util.o -o app # several inputs, one executable
./main -target=x86_64 -c a.c                # a.o in the current directory
./main -target=aarch64 -S a.c -o a.s        # assembly only
./main -E -DDEBUG=1 a.c                     # preprocessed C on stdout
echo 'int main(){return 3;}' | ./main -target=x86_64 - -o three
./main --help
```

| Option | Meaning |
|---|---|
| `-o FILE` | output (default `a.out`; `NAME.o` / `NAME.s` with `-c` / `-S`) |
| `-c`, `-S`, `-E` | stop after assembling, after compiling, after preprocessing |
| `-I DIR` | include directory, before pc_includes and `/usr/include` |
| `-D NAME[=VALUE]`, `-U NAME` | define (VALUE defaults to 1) / undefine a macro |
| `-target=T` | `x86` (default), `x86_64`, `aarch64`, `riscv64`, `samos-*` |
| `-sysroot=DIR` | SamOS sysroot for the `samos-*` targets |
| `-v` | print the assembler and linker commands |
| `-` | read the source from standard input |

Options may come before or after the file names. `.c` inputs are
compiled, `.o` and `.a` inputs go to the linker; an unknown option or
file type is an error. This form is quiet: nothing on stdout but `-E`
output, errors on stderr, exit status 0 or non-zero. Intermediate files
live in a private directory under `$TMPDIR` and are removed; the output
is written to a temporary next to it and renamed into place, so a
failed build leaves nothing behind.

The rest of this guide describes the original form,
`./main [-target=X] in out [object]`, which still works exactly as
before (the tests, `tools/gcc_differential.sh` and SamOS's tools use
it). It is chosen when none of `-o`, `-c`, `-S`, `-E` is given and the
second file name is not itself an input (`.c`, `.o`, `.a`, `-`), so
`./main a.c b.c` links both files into `a.out`. With no arguments the
driver prints its usage instead of compiling `./test.c`.

## The pipeline at a glance

```
my.c
  |
  | ./main my.c my.bin
  |
  v
+----------------------------------+
| SamCompiler (./main)             |
|                                  |
|   lex     -> tokens              |
|   preproc -> tokens (expanded)   |
|   parse   -> AST                 |
|   validate                       |
|   codegen -> asm text            |
|     (backend chosen by -target)  |
+----------------------------------+
  |
  |  writes:  my.bin   (asm source, temporarily named .bin)
  v
+----------------------------------+
| main.c then shells out (x86):    |
|   nasm -f elf32 my.bin -o my.bin.o          (ELF32 .o file)
|   i686-linux-gnu-gcc -static my.bin.o -o my.bin
|                       (links the binary, overwriting the asm
|                        at my.bin; gcc -m32 if the i686 cross
|                        gcc is missing,)
+----------------------------------+
  |
  v
my.bin  (executable; chmod +x already)
my.bin.o  (intermediate object; can delete)
```

That's it. Five passes inside `./main`, then two child processes
(the assembler and the linker). The other targets swap the
backend, the assembler and the linker; see "Choosing a target"
below.

## What main.c actually does

`main.c` is a thin CLI driver. For the original form it runs
`main_legacy`; the interesting parts:

```c
int compile_flags = COMPILE_PROCESS_EXECUTE_NASM;
if(S_EQ(option, "object")){
    compile_flags |= COMPILE_PROCESS_EXPORT_AS_OBJECT;
}
```

- `COMPILE_PROCESS_EXECUTE_NASM` is on by default. It tells the
  driver to run `nasm` after the compiler is done.
- `COMPILE_PROCESS_EXPORT_AS_OBJECT` (set when arg 3 is the
  literal string `object`) tells the driver to **stop after
  the assembler**, i.e. skip the link step. The assembler still
  runs.

Then:

```c
int res = compile_file(input_file, main_tmp_output, compile_flags);
```

`main_tmp_output` is a temporary file next to `output_file`
(`output_file.XXXXXX`, from `mkstemp`). It is renamed over
`output_file` only when compilation succeeds, so a failed compile
never empties an existing output. If the output is the input file
itself, the driver refuses before compiling:

```
$ ./main my.c my.c
The output file my.c is the input file, refusing to overwrite it
```

That's the compiler proper. `compile_file` lives in `compiler.c`
and runs the five passes inside `./main`:

1. **`compile_process_create`** - opens the source, builds the
   per-translation-unit state (token vectors, node vectors,
   include dirs, preprocessor, default resolver).
2. **`lex_process_create` + `lex()`** - lexer. Reads characters,
   produces tokens (numbers, identifiers, keywords, operators,
   strings).
3. **`preprocessor_run()`** - preprocessor. Walks the original
   token stream, handles `#define`, `#include`, `#ifdef`, macros,
   `##`, `#`, native functions like `__LINE__` / `va_start`.
   Output is a fresh token vector with everything expanded.
4. **`parse()`** - parser. Consumes the expanded tokens, builds
   an AST (NODE_TYPE_*). Resolves operator precedence.
5. **`validate()`** - validator. Runs on the AST the parser just
   built (this guide used to list it before `parse()`, which is
   not the order in `compile_file`). Catches duplicate function /
   variable / struct / union definitions and void-return-with-value
   errors. It checks only variable, `return` and `if` statements,
   and an undeclared identifier only when it is the whole `return`
   expression. Lives in
   `validator.c`.
6. **`codegen()`** - codegen. `codegen.c` hands the AST to the
   target's backend (`codegen_x86.c`, `codegen_x86_64.c`,
   `codegen_aarch64.c` or `codegen_riscv64.c`), which emits asm
   text directly to `output_file`. Per-function
   prologues/epilogues, stackframe tracking, string-table dedup,
   switch compare chains, calls - all in the backend.

If `res == COMPILER_FILE_COMPILED_OK`, prints
`everything compiled fine`. Otherwise prints `Compile failed`
and returns.

Then the post-codegen step (still inside `main.c`). Since the
-target flag, the commands come from the target descriptor in `target.c`
(`asm_cmd`, `link_cmd`, `link_fallback_cmd`):

```c
if(compile_flags & COMPILE_PROCESS_EXECUTE_NASM){
    snprintf(nasm_output_file, sizeof(nasm_output_file), "%s.o", output_file);
    // both paths are single-quoted for /bin/sh first
    main_shell_quote(output_file, quoted_output, sizeof(quoted_output));
    main_shell_quote(nasm_output_file, quoted_object, sizeof(quoted_object));
    snprintf(asm_cmd, sizeof(asm_cmd), target->asm_cmd, quoted_output, quoted_object);
    if(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT){
        // object-mode: stop after the assembler
        snprintf(nasm_cmd, sizeof(nasm_cmd), "%s", asm_cmd);
    } else {
        // default: assemble, then link
        snprintf(link_cmd, sizeof(link_cmd), main_link_cmd(target), quoted_object, quoted_output);
        snprintf(nasm_cmd, sizeof(nasm_cmd), "%s && %s", asm_cmd, link_cmd);
    }
    printf("%s\n", nasm_cmd);
    int rc = system(nasm_cmd);
    if(rc != 0){
        printf("Issue assemblign the assembly file with NASM and linking with gcc\n");
        // "assemblign" is the upstream typo, preserved verbatim.
        // rc is a wait status: return the tool's exit status, or 1.
        ...
    }
    // cross targets: "run with: qemu-aarch64 <out>" and similar
}
```

Paths are quoted, so a path with spaces or shell characters is
passed to the assembler and linker as one argument and never run
as a command. A failed assemble or link makes `./main` exit
non-zero, so scripts and `make` see the failure.

Key thing to understand: **`output_file` is reused at three
different points in the pipeline.** Initially the compiler
writes the asm source there. The assembler then reads from it and
writes the `.o` to `output_file + ".o"`. Finally the linker reads
the `.o` and writes the linked ELF binary back to `output_file`,
OVERWRITING the asm. So at the end (x86):

| `option` arg | Contents of `output_file` | Contents of `output_file.o` |
|---|---|---|
| (omitted, default) | linked 32-bit ELF executable | ELF32 object |
| `object` | NASM source | ELF32 object |

## Choosing a target

```
./main [-target=x86|x86_64|aarch64|riscv64] <input.c> <output_path> [object]
```

`-target=` may come anywhere on the command line. Without it the
target is `x86` and everything behaves as before the multi-target
work.

| `-target=` | Output | Assembler | Linker | Run | Guide |
|---|---|---|---|---|---|
| `x86` | NASM, 32-bit | `nasm -f elf32` | `i686-linux-gnu-gcc -static` (else `gcc -m32`) | native | [x86](targets/x86.md) |
| `x86_64` | NASM, 64-bit, `default rel` | `nasm -f elf64` | `gcc -no-pie` | native | [x86_64](targets/x86_64.md) |
| `aarch64` | GNU as | `aarch64-linux-gnu-as` | `aarch64-linux-gnu-gcc -static` | `qemu-aarch64` | [aarch64](targets/aarch64.md) |
| `riscv64` | GNU as | `riscv64-linux-gnu-as` | `riscv64-linux-gnu-gcc -static` | `qemu-riscv64` | [riscv64](targets/riscv64.md) |

For the cross targets the driver prints how to run the result:

```
$ ./main -target=aarch64 /tmp/g/add.c /tmp/g/add_a64
...
everything compiled fine
aarch64-linux-gnu-as /tmp/g/add_a64 -o /tmp/g/add_a64.o && aarch64-linux-gnu-gcc -static /tmp/g/add_a64.o -o /tmp/g/add_a64
run with: qemu-aarch64 /tmp/g/add_a64
```

An unknown name is rejected:

```
$ ./main -target=sparc tests/golden/inputs/01_return_const.c /tmp/d2
Unknown target sparc, expected x86, x86_64, aarch64, riscv64, samos-x86_64, samos-aarch64 or samos-riscv64
```

The preprocessor predefines `__x86_64__`, `__aarch64__` or
`__riscv` (with `__riscv_xlen` = 64) for the 64-bit targets, and
`__LP64__` on all three; x86 adds none.

`-I<dir>` (or `-I <dir>`) adds an include directory, searched
before the defaults; `-D` and `-U` work in either form.

### SamOS programs

`-target=samos-x86_64`, `samos-aarch64` and `samos-riscv64` build a
static ELF for SamOS against a SamOS sysroot (SamOS's Makefile writes
`build/<arch>/sysroot`), given as `-sysroot=DIR` or `SAMOS_SYSROOT`.
They predefine `__samos__` as well, take headers from
`DIR/include` and `pc_includes` only, and link `DIR/lib/crt0.o`, the
program and `DIR/lib/libsamos.a` with `DIR/lib/user.ld`.

```
./main -target=samos-riscv64 -sysroot=$SAMOS/build/riscv64/sysroot hello.c HELLO.ELF
```

That second column is the one you read to debug codegen. The
first is the one you `./output_file` to run.

## CLI patterns

```
./main <input.c> <output_path> [object]
```

| What you want | Original form | gcc-style |
|---|---|---|
| The asm, no link (the old form still assembles it, leaving `out.o`) | `./main my.c /tmp/my.asm object` | `./main -S my.c -o /tmp/my.asm` |
| Linked 32-bit binary (default) | `./main my.c /tmp/my.bin` | `./main my.c -o /tmp/my.bin` |
| Compile + run | `./main my.c /tmp/my.bin && /tmp/my.bin; echo "exit=$?"` | `./main my.c -o /tmp/my.bin && /tmp/my.bin` |
| Inspect the asm | `./main my.c /tmp/my.asm object && less /tmp/my.asm` | `./main -S my.c && less my.s` |
| An object file only | (`out.o` next to the asm) | `./main -c my.c` |
| See the preprocessor output | | `./main -E my.c` |

In the original form the third positional arg is the toggle:
- `object` -> stops after the assembler. The `output_file` holds
  the asm source; the `.o` is next to it; no binary is produced.
- omitted -> full pipeline through gcc; final binary at
  `output_file`, `output_file.o` left next to it.
- anything else -> an error (it used to mean "link").

The asm is no longer printed on stdout as it is generated; read the
output file.

## Drive it from your own .c file

### 1. Build the compiler once

```
cd ~/projects/samcompiler
./build.sh
```

Produces `./main`. Rebuild only when the compiler source
changes; you can build any number of programs with the same
`./main` afterwards.

### 2. Drop your source anywhere

Easiest: put it under `samples/` so the runner picks it up:

```
samples/15_my_thing.c
```

Add a magic comment at the top so the runner knows what exit
code to expect:

```c
// EXPECTED EXIT: 42

int main()
{
    return 42;
}
```

The runner greps for `EXPECTED EXIT: <N>` on the first line. If
the line is missing the runner still compiles + runs but won't
verify the exit code.

### 3. Compile it

Quick check (asm only, no link):

```
./main samples/15_my_thing.c /tmp/15.asm object
less /tmp/15.asm
```

Full pipeline (binary + run):

```
./main samples/15_my_thing.c /tmp/15.bin
/tmp/15.bin
echo "exit=$?"
```

Or just let the sample runner do it:

```
bash samples/run_all.sh
```

Your file shows up in the table automatically.

### 4. Use the includes you ship

Anything you `#include` resolves in this order
, whatever the current directory:

```
the including file's directory      ("x.h" only, not <x.h>)
each -I directory, in order
<directory of ./main>/pc_includes   (found through /proc/self/exe)
/usr/include
```

A samos target replaces the last two with `<sysroot>/include` and
pc_includes. pc_includes declares the common C library
: `stdio.h`, `stdlib.h`, `string.h`, `ctype.h`,
`errno.h`, `limits.h`, `stdint.h`, `stddef.h`, `stdbool.h`,
`stdarg.h` and `assert.h`, with glibc's signatures, so a program
using them links against the target's C library.

To ship a header for your program, put it next to your source and
`#include "my_header.h"`, or pass `-I dir`. Sample 07 includes
`values.h` from `pc_includes/`.

## Debug recipes

| Symptom | What to try |
|---|---|
| `Compile failed` | The compiler's own parser / validator bailed at the first error. Read the message on stderr: it ends with `on line L, col C in file F` (not `file:line:col`), the place the compiler had read to, which can be past the real fault. |
| `Issue assemblign...` (after `everything compiled fine`) | The emitted asm is bad OR the link step failed. Re-run with `object` and inspect the `.asm`. |
| `Issue assemblign...` after a clean asm | No 32-bit linker. Install `gcc-i686-linux-gnu` (not `gcc-multilib`, which removes the cross compilers;). For the other targets, see the target guide's prerequisites. |
| `unknown option` / `unknown file type` | The gcc-style form takes the options in `./main --help`, and `.c`, `.o`, `.a` or `-` as inputs. |
| `unknown mode X` | The original form's third argument can only be `object`. |
| `./main: No such file or directory` | The chapter test suite (or `make clean`) deleted it. Just `./build.sh` again. |
| Exit code is wrong, compile succeeded | Codegen quirk. Check the `Known codegen quirks` section in `samples/README.md` - the patterns there are real (bare `int[]` reads return addresses, void-return loads, comparison right-operand drops). |
| `cannot find -lgcc` | `gcc -m32` was used without its 32-bit runtime. Install `gcc-i686-linux-gnu`; the driver then links with it. |

## See also

- `RUNNING_TESTS.md` - the chapter test suite (compiler internals)
- `FEATURE_TESTING.md` - the sample programs (compiler outputs)
- `DEBUGGING.md` - failure shapes we've already encountered
- `samples/README.md` - the sample list + codegen quirks
