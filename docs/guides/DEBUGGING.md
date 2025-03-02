# Debugging guide

A retrospective on the failure shapes we ran into while building
SamCompiler chapter by chapter, and the moves that worked. None of
this is theoretical; every entry maps to a real incident in the
git log.

## Categories at a glance

| Category | How it looks | First move |
|---|---|---|
| WSL connection dropped | `Wsl/Service/0x8007274c`, garbled char-spread output | `wsl --shutdown && sleep 3`, re-run |
| Build cache desync | `Text file busy`, missing `.o` files mid-suite | Retry the suite once; clean rebuild if it persists |
| `/tmp/peach_ref` gone | `cd /tmp/peach_ref` fails | Re-clone from `nibblebits/PeachCompiler` |
| Probe silently exits 0 | `<test> ... ok` with no echo, exit 0 but no output | Binary segfaulted; `set -e` killed the script silently |
| Suite race | `01-builds` or random later test fails once, passes on retry | Retry; if it sticks, hunt the actual cause |
| Stale notification | `task-notification status=failed` for a task that already finished | Acknowledge briefly, keep going |
| Enum value shift | Hard-coded `type=8` expectations break when a chapter inserts an enum slot | Bump every later expectation by the offset and add a comment |
| Forward decl warning | `struct X declared inside parameter list will not be visible` | Add a forward `struct X;` before the typedef block |
| Heredoc shell escape | `cat >> file <<EOF ... CHEOF` mangles `'` / `(` inside the body | Use Write tool to a tmp file then `cat tmp >> target` |
| Upstream bug | Chapter ships an `assert(x = 0)` typo, missing closing quote, double-pushed brace, etc. | Deviate, document under `docs/NN-...md`, move on |

## Concrete incidents

### 1. The "silent" failing test

`set -euo pipefail` in `tests/lib.sh` plus a probe binary that
segfaults inside `got="$(./bin)"` produces a test that prints
nothing and may exit 0 from the parent shell's view. The
`assert_contains` line never runs because `set -e` already killed
the script.

How to recognize: the test echoes the build commands (under
`bash -x`) but never the `+ assert_contains` line; the EXIT trap
fires and `rm -f /tmp/sam_*` runs.

How to fix: instrument the probe with `fprintf(stderr, ...)` at
each suspect point; capture stdout AND stderr through
`got="$("$bin" 2>&1 || true)"`; write tmp output to files
explicitly so the buffering can't hide a partial print before the
crash.

Real example: test 126 (`( 1 ) + 2` for parse_parentheses) hit
this because `parse_exp` unconditionally fell through to
`parse_for_operator`, which null-dereferenced after
`deal_with_additional_expression` consumed the trailing tokens.
We added a non-null + OPERATOR guard before the fallthrough.

### 2. The build cache race

Running the full suite (`bash tests/run-all.sh`) sometimes
produces `Text file busy` on `./main` or `/usr/bin/ld: cannot find
build/foo.o` partway through. Each test's `lib.sh` calls
`./build.sh` and captures `LINK_OBJS` after; if multiple tests
land too close together the object files get shuffled.

Resolution: retry the suite once. If the same test fails twice in
a row, run it alone with `bash tests/<n>-foo.sh` to confirm
whether it's a real failure or a race.

### 3. `compile_process_create` signature change (ch200)

ch200 added a `parent_process` parameter to
`compile_process_create`. That immediately broke 62 existing test
probes that hand-call it. Rather than adding a 3-arg wrapper, we
ran:

```
grep -rl 'compile_process_create(' tests/ | xargs \
    sed -i 's|compile_process_create(\([^)]*\))|compile_process_create(\1, NULL)|g'
```

Then fixed the ONE probe (the ch200 test itself) the sed had
double-bumped. Lesson: bulk sed across `tests/` is fine; just
audit the chapter test that already takes the new arg.

### 4. Enum slot insertion (ch237)

ch237 inserted `RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION` between
`FUNCTION` (slot 1) and `STRUCTURE`. Every downstream slot bumped
by 1. Four tests broke: 69, 70, 79, 80. They all hard-coded the
old slot numbers in `assert_contains` lines.

The fix: bump each expected value by 1 AND add an inline comment
saying "post-chXXX" so future readers know why.

The lesson: probes that read enum values should ideally include
the value's symbolic name in the asserted string (`GENERAL=7
unknown entity (post-ch237)`). Pure-numeric asserts age badly.

### 5. The `__LINE__` default definition (ch227)

ch227 changed `preprocessor_initialize` to call
`preprocessor_create_definitions` which registers `__LINE__` as a
native built-in. Suddenly six tests that assumed `vector_count(
preprocessor->definitions) == 0` (or peeked from index 0) broke.

Fix: each test now subtracts the native count or peeks from index
1. Documented as "ch228 followup: adapt 6 prior tests for the
__LINE__ default definition" in the git log.

### 6. Upstream code we couldn't ship verbatim

The book ships some chapters with bugs that prevent the project
from building or running. We deviate, write a `docs/NN-...md`
note explaining what we changed and why, and keep going. The
running list:

- ch194: `assert(left_node_type = 0)` always fires. Fixed to
  `>= 0`.
- ch195: `parse_exp` falls through to `parse_for_operator` even
  when the rest of the input was consumed by
  `deal_with_additional_expression`. Guarded.
- ch200: `preprocessor_handle_token` switch had no cases at all.
  Added a `default: push_through` arm so the rest of the pipeline
  works until later chapters fill in real handling.
- ch203: macro arg parser does `vector_push(args, (void*)sval)`
  which copies the first 8 bytes of the string into the slot
  rather than the pointer. Preserved verbatim; tests just count
  args instead of dereferencing them.
- ch217 macro-call test was outdated by ch218 substitution. Tests
  updated, doc explains.
- ch220 typedef-struct body: upstream peeks the leading `{` but
  never consumes it, so `for_brackets` reads it again and pushes
  two `{`. We consume it explicitly.

### 7. `Build.sh: Permission denied` after WSL restart

If WSL is shut down mid-build, `build.sh` can come back without
the execute bit. Symptom: tests start failing with `./build.sh:
Permission denied`. Fix: `chmod +x ./build.sh`.

### 8. The heredoc shell-escape footgun

Appending C code to an existing `.c` file with `bash -c "cat >>
file <<EOF ..."` mangles single quotes, parens in comments, and
`\\n` inside string literals. The shell evaluates the body before
writing it.

Two reliable workarounds:

- Use the editor / Write tool to drop content into a tmp file,
  then `cat tmp >> target.c` from a clean shell.
- Avoid `bash -c "..."` wrapping a heredoc entirely; write the
  whole script to a file first, then `bash /tmp/script.sh`.

## Debugging workflow

When a chapter test fails the routine is:

1. **Read the printed assertion.** `assert_contains` prints both
   the expected substring and the actual stdout. Half the time
   that's enough.
2. **Re-run isolated.** `bash tests/<n>-foo.sh`. If it passes,
   suspect the suite race.
3. **Capture stderr.** Patch the test to `got="$("$bin" 2>&1 ||
   true)"` and print it under `DEBUG: $got >&2`. Most segfaults
   leave a clear `Assertion ... failed` or `Segmentation fault`
   line on stderr before they die.
4. **Trace the chapter diff.** `cd /tmp/peach_ref && git show
   <commit>` next to your local file. Reformatting / whitespace
   differences from upstream are easy to spot, so are typos.
5. **Add a deviation note.** If you have to depart from upstream
   verbatim, write the deviation into the chapter's
   `docs/NN-...md` BEFORE you commit. Future-you reading the
   commit log will need it.

See also `RUNNING_TESTS.md` for how to run the suite and what the
per-test output looks like.
