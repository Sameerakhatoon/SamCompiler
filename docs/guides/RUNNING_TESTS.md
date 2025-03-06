# Running tests

Every chapter ships with a bash E2E test under `tests/`. The
suite is the primary "did I break anything" signal. This guide
covers how to build, run, and read those tests.

## Build the compiler

```
cd ~/projects/samcompiler
./build.sh
```

This runs `make clean`, creates the `build/`, `build/helpers/`,
and `build/static-includes/` directories, then `make all`. It
produces `./main` plus one `.o` per translation unit under
`build/`.

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
passed: 175   failed: 0
```

Non-zero `failed` -> exit code 1, and the runner lists the failed
test names below the summary.

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

Write a real `.c` file, run `./main src.c out.asm`, and check the
emitted asm or the binary the linker built. The `./main` binary
forks `nasm` and `gcc -m32` itself; the test captures combined
stdout / stderr and pattern-matches.

Look for the `nasm` / `gcc -m32` setup if you want to actually
run the produced binary:

```bash
command -v nasm >/dev/null || skip
gcc -m32 -c -o /tmp/_check.o -x c /dev/null 2>/dev/null || skip
```

Tests gracefully skip when 32-bit toolchain isn't installed.

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
  own `.c` file (CLI patterns, what each pipeline stage does,
  the `nasm` + `gcc -m32` shell-out)
- `DEBUGGING.md` for failure shapes and how to investigate them
- `FEATURE_TESTING.md` for end-to-end feature verification using
  the sample programs in `samples/`
