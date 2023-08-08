# Using the compiler

What actually happens when you run `./main my.c my.bin`, and how
to drive it against your own programs.

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

`main.c` is a thin CLI driver. The interesting parts:

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
  `nasm`**, i.e. skip the `gcc -m32` link step.

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
   variable / struct / union definitions, void-return-with-value
   errors, missing identifiers. Lives in `validator.c`.
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

`-target=` must come before the file names. Without it the target
is `x86` and everything behaves as before the multi-target work.

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
Unknown target sparc, expected x86, x86_64, aarch64 or riscv64
```

The preprocessor predefines `__x86_64__`, `__aarch64__` or
`__riscv` (with `__riscv_xlen` = 64) for the 64-bit targets, and
`__LP64__` on all three; x86 adds none.

That second column is the one you read to debug codegen. The
first is the one you `./output_file` to run.

## CLI patterns

```
./main <input.c> <output_path> [object]
```

| What you want | Command |
|---|---|
| Just the asm (no nasm, no gcc) | `./main my.c /tmp/my.asm object` |
| Linked 32-bit binary (default) | `./main my.c /tmp/my.bin` |
| Compile + run | `./main my.c /tmp/my.bin && /tmp/my.bin; echo "exit=$?"` |
| Inspect the asm | `./main my.c /tmp/my.asm object && less /tmp/my.asm` |
| Inspect the asm AND link | `./main my.c /tmp/my.asm object; cp /tmp/my.asm /tmp/inspect.asm; ./main my.c /tmp/my.bin` |

The third positional arg is the toggle:
- `object` -> stops after `nasm`. The `output_file` holds the
  NASM source; the `.o` is next to it; no binary is produced.
- anything else (or omitted) -> full pipeline through gcc; final
  binary at `output_file`.

If you want both the asm AND the binary, run the command twice
with different output paths.

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

Anything you `#include` resolves via the search list set in
`cprocess.c`:

```
./pc_includes
../pc_includes
/usr/include/peach-includes
/usr/include
```

To ship a header for your program, drop `my_header.h` into
`pc_includes/` and `#include <my_header.h>` from your source.
Sample 07 demonstrates this exact pattern with `values.h`.

## Debug recipes

| Symptom | What to try |
|---|---|
| `Compile failed` | The compiler's own parser / validator bailed. Re-run and read the diagnostic line - it includes line:col and a message. |
| `Issue assemblign...` (after `everything compiled fine`) | The emitted asm is bad OR the link step failed. Re-run with `object` and inspect the `.asm`. |
| `Issue assemblign...` after a clean asm | No 32-bit linker. Install `gcc-i686-linux-gnu` (not `gcc-multilib`, which removes the cross compilers;). For the other targets, see the target guide's prerequisites. |
| `./main: No such file or directory` | The chapter test suite (or `make clean`) deleted it. Just `./build.sh` again. |
| Exit code is wrong, compile succeeded | Codegen quirk. Check the `Known codegen quirks` section in `samples/README.md` - the patterns there are real (bare `int[]` reads return addresses, void-return loads, comparison right-operand drops). |
| `cannot find -lgcc` | `gcc -m32` was used without its 32-bit runtime. Install `gcc-i686-linux-gnu`; the driver then links with it. |

## See also

- `RUNNING_TESTS.md` - the chapter test suite (compiler internals)
- `FEATURE_TESTING.md` - the sample programs (compiler outputs)
- `DEBUGGING.md` - failure shapes we've already encountered
- `samples/README.md` - the sample list + codegen quirks
