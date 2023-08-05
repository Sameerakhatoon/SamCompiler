# Running tests

Every chapter ships with a bash E2E test under `tests/`. The
suite is the primary "did I break anything" signal. This guide
covers how to build, run, and read those tests.

## Build the compiler

```
cd ~/projects/samcompiler
./build.sh
```

This creates the `build/`, `build/helpers/` and
`build/static-includes/` directories, then runs `make -j"$(nproc)"
all`. It produces `./main` plus one `.o` and one `.d` per translation
unit under `build/`. The `.d` files come from gcc `-MMD` and list the
headers each object read, so the build is incremental and still
correct after a header edit. Every test calls `build.sh` (twice,
through `lib.sh`); after the first build those calls do nothing.

`./build.sh clean` forces a rebuild from scratch.

If `build.sh` itself reports `Permission denied`:

```
chmod +x ./build.sh
./build.sh
```

## Run the full suite

```
bash tests/run-all.sh
```

The runner walks `tests/[0-9]*.sh` alphabetically (so `100-...`
sorts between `19-...` and `20-...` per the shell glob, not
numerically). Each test prints either:

```
<name> ... ok
```

or

```
<name> ... FAIL: <message>
```

The suite ends with a summary:

```
passed: 189   failed: 0
```

Non-zero `failed` -> exit code 1, and the runner lists the failed
test names below the summary.

## Tests per target

Most tests exercise the front end and need no assembler at all. The
ones below are the multi-target tests and the older
end-to-end test; "compile" means only `./main` runs, "run" means the
program is assembled, linked and executed.

| Test | x86 | x86_64 | aarch64 | riscv64 | Needs |
|---|---|---|---|---|---|
| `90-end-to-end-nasm` | assemble | | | | nasm and `i686-linux-gnu-gcc` or `gcc -m32`, else skips |
| `176-x86-golden` | compile, byte-for-byte diff | | | | nothing extra |
| `177-target-descriptor` | descriptor | | | | nothing extra |
| `178-target-flag` | `-target=x86` = default | descriptor + macros | descriptor + macros | descriptor + macros | nothing extra |
| `179-backend-split` | backend, rdefault, stdarg hooks | | | | nothing extra |
| `180-left-assoc` | `a - b - c` asm order, run input 41 | | | | nasm for the run part, else skips it |
| `181-x86-run` | run 55 programs | | | | nasm and `i686-linux-gnu-gcc` or `gcc -m32`, else skips |
| `200-x86-64-codegen` | | compile, asm shape | | | nothing extra |
| `201-x86-64-run` | | run 55 programs | | | nasm, gcc; skips without nasm |
| `210-aarch64-codegen` | | | compile, asm shape | | nothing extra |
| `211-aarch64-run` | | | run 55 programs | | `gcc-aarch64-linux-gnu`, `qemu-user`; skips without them |
| `220-riscv64-codegen` | | | | compile, asm shape | nothing extra |
| `221-riscv64-run` | | | | run 55 programs | `gcc-riscv64-linux-gnu`, `qemu-user`; skips without them |
| `230-samples-all-targets` | run samples | run samples | run samples | run samples | cross targets only when their gcc and qemu exist |
| `240-compare-gcc` | when `i686-linux-gnu-gcc` exists | always | when the cross gcc exists | when the cross gcc exists | gcc per target |
| `250-gcc-differential` | run `tests/diff`, compare with gcc | same | same | same | gcc and qemu per target; fails if one is missing |

The "55 programs" are the 41 golden inputs in
`tests/golden/inputs/` plus the 14 samples.

Run one target's tests on their own:

```
bash tests/210-aarch64-codegen.sh
bash tests/211-aarch64-run.sh
```

and the samples for one or all targets:

```
bash samples/run_all.sh ARCH=aarch64
bash samples/run_all.sh ARCH=all
```

Compare any directory of programs with gcc on every target (the
reference is each target's own gcc):

```
bash tools/gcc_differential.sh tests/diff
bash tools/gcc_differential.sh /path/to/programs x86_64 riscv64
```

The x86 golden test must stay green whatever changes; if a change
to x86 output is intended, regenerate with
`bash tests/golden/regen.sh x86` and commit the new files with the
reason.

## Run a single test

```
bash tests/162-sizeof.sh
```

Per-test exit code: 0 = ok, 1 = FAIL. `lib.sh` sets `set -euo
pipefail`, so a probe segfault inside `got="$($bin)"` will
short-circuit the script silently (see DEBUGGING.md "the silent
failing test"). If you suspect that's happening, capture stderr:

```
bash -x tests/162-sizeof.sh 2>&1 | tail -30
```

## Test layout

Each test under `tests/`:

```bash
#!/usr/bin/env bash
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_chNNN_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_chNNN_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include "compiler.h"
... probe body that exercises the feature ...
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null \
    || fail "ch... probe failed to compile"

got="$("$bin")"
assert_contains "$got" "expected substring" "what we are testing"
pass
```

Key helpers from `tests/lib.sh`:

- `REPO_ROOT` - absolute path to the repo root
- `LINK_OBJS` - whitespace-separated list of every `.o` under
  `build/`, `build/helpers/`, `build/static-includes/`
- `fail "<msg>"` - print and exit 1
- `pass` - print `<name> ... ok` and exit 0
- `assert_eq want got [what]`
- `assert_contains haystack needle [what]`

## Two flavors of test

### Probe tests (most of them)

Build a tiny `main(void)` against the compiler's `.o` files and
call internal APIs directly (`expressionable_create`,
`preprocessor_run`, `compile_process_create`, ...). These exercise
the compiler as a library and capture stdout from the probe.

### End-to-end source tests

Write a real `.c` file, run `./main [-target=...] src.c out`, and
check the emitted asm or the binary the linker built. The `./main`
binary forks the target's assembler and linker itself (for x86
`nasm` and `i686-linux-gnu-gcc -static`); the test captures
combined stdout / stderr and pattern-matches.

Tests that run a binary check for their tools first and skip
cleanly without them, for example `tests/211-aarch64-run.sh`:

```bash
for tool in aarch64-linux-gnu-as aarch64-linux-gnu-gcc qemu-aarch64; do
    command -v "$tool" >/dev/null || { echo "211-aarch64-run ... skip (no $tool)"; exit 0; }
done
```

`lib.sh` runs every test with `set -euo pipefail`. A loop that runs
programs must guard their exit codes (`out=$(prog) && ec=0 || ec=$?`)
and `grep`s that may match nothing (`... | head -1 || true`), or
the test ends silently at the first non-zero status.

## When the suite is flaky

The full suite can hit a build cache race (see DEBUGGING.md). The
canonical workflow:

1. `bash tests/run-all.sh`
2. If exit 1, re-run once. Real failures stick.
3. If a specific test still fails: `bash tests/<n>-foo.sh`
   isolated. Now you have a clean reproduction.

## Adding a new test

Number convention: `tests/NNN-short-name.sh` where `NNN` matches
the chapter number you're verifying (or the next free integer for
a fix-up test). Copy the closest existing test as a starting
point, swap in your probe, document the assertion at the top.

The runner picks up new files automatically; no registry to
update.

## What a "good" assertion looks like

The asserted substring should encode both the symbolic name and
its value, e.g.:

```bash
assert_contains "$got" "PREPROCESSOR_NUMBER_NODE=0 fmt=lld" \
    "NUMBER node carries the llnum value"
```

Pure-numeric asserts (`type=4`) age badly across chapters that
shuffle enum slots (ch237 inserted `NATIVE_FUNCTION` between
`FUNCTION` and `STRUCTURE`, bumping everything later by 1). When
you do hard-code an enum value, leave a comment naming the chapter
that fixed it in place.

## See also

- `USING_THE_COMPILER.md` for how to drive `./main` against your
  own `.c` file (CLI patterns, `-target=`, what each pipeline stage
  does, the assembler and linker shell-out)
- `targets/*.md` for each target's toolchain, run and debug steps
- `DEBUGGING.md` for failure shapes and how to investigate them
- `FEATURE_TESTING.md` for end-to-end feature verification using
  the sample programs in `samples/`
