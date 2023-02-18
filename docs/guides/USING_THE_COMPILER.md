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
|   validate                       |
|   parse   -> AST                 |
|   codegen -> NASM text           |
+----------------------------------+
  |
  |  writes:  my.bin   (NASM source, temporarily named .bin)
  v
+----------------------------------+
| main.c then shells out:          |
|   nasm -f elf32 my.bin -o my.bin.o   (ELF32 .o file)
|   gcc -m32 my.bin.o -o my.bin        (links to 32-bit ELF binary,
|                                       overwriting the .asm at my.bin)
+----------------------------------+
  |
  v
my.bin  (executable; chmod +x already)
my.bin.o  (intermediate object; can delete)
```

That's it. Five passes inside `./main`, then two child processes
(`nasm` and `gcc -m32`).

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
int res = compile_file(input_file, output_file, compile_flags);
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
4. **`validate()`** - validator. Catches duplicate function /
   variable / struct / union definitions, void-return-with-value
   errors, missing identifiers. Lives in `validator.c`.
5. **`parse()`** - parser. Consumes the expanded tokens, builds
   an AST (NODE_TYPE_*). Resolves operator precedence.
6. **`codegen()`** - codegen. Walks the AST and emits NASM text
   directly to `output_file`. Per-function prologues/epilogues,
   stackframe tracking, string-table dedup, switch jump tables,
   indirect-call slots - all here.

If `res == COMPILER_FILE_COMPILED_OK`, prints
`everything compiled fine`. Otherwise prints `Compile failed`
and returns.

Then the post-codegen step (still inside `main.c`):

```c
if(compile_flags & COMPILE_PROCESS_EXECUTE_NASM){
    snprintf(nasm_output_file, ..., "%s.o", output_file);
    if(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT){
        // object-mode: stop after the assembler
        snprintf(nasm_cmd, ..., "nasm -f elf32 %s -o %s",
                 output_file, nasm_output_file);
    } else {
        // default: assemble, then link
        snprintf(nasm_cmd, ...,
                 "nasm -f elf32 %s -o %s && gcc -m32 %s -o %s",
                 output_file, nasm_output_file,
                 nasm_output_file, output_file);
    }
    int rc = system(nasm_cmd);
    if(rc != 0){
        printf("Issue assemblign the assembly file with NASM and linking with gcc\n");
        // "assemblign" is the upstream typo, preserved verbatim.
        return rc;
    }
}
```

Key thing to understand: **`output_file` is reused at three
different points in the pipeline.** Initially the compiler
writes the NASM source there. `nasm` then reads from it and
writes the `.o` to `output_file + ".o"`. Finally `gcc -m32`
reads the `.o` and writes the linked ELF binary back to
`output_file`, OVERWRITING the asm. So at the end:

| `option` arg | Contents of `output_file` | Contents of `output_file.o` |
|---|---|---|
| (omitted, default) | linked 32-bit ELF executable | ELF32 object |
| `object` | NASM source | ELF32 object |

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
| `Issue assemblign...` after a clean asm | `gcc-multilib` isn't installed. `sudo apt install gcc-multilib`. |
| `./main: No such file or directory` | The chapter test suite (or `make clean`) deleted it. Just `./build.sh` again. |
| Exit code is wrong, compile succeeded | Codegen quirk. Check the `Known codegen quirks` section in `samples/README.md` - the patterns there are real (bare `int[]` reads return addresses, void-return loads, comparison right-operand drops). |
| `cannot find -lgcc` | 32-bit C runtime missing. `sudo apt install gcc-multilib`. |

## See also

- `RUNNING_TESTS.md` - the chapter test suite (compiler internals)
- `FEATURE_TESTING.md` - the sample programs (compiler outputs)
- `DEBUGGING.md` - failure shapes we've already encountered
- `samples/README.md` - the sample list + codegen quirks
