#!/usr/bin/env bash
# the gcc-style driver. -o, -c, -S, -E,
# several inputs compiled and linked together, .o inputs, - for
# standard input, -target= anywhere, unknown options rejected, usage
# without arguments, no asm on stdout, and no intermediate files left.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_driver.XXXXXX)
export TMPDIR="$tmp/tmpdir"
mkdir -p "$TMPDIR"
trap 'rm -rf "$tmp"' EXIT
M="$REPO_ROOT/main"
cd "$tmp"

cat > a.c <<'EOF'
int helper(int x);
int main(){ return helper(40); }
EOF
cat > b.c <<'EOF'
int helper(int x){ return x + 2; }
EOF

# No arguments: usage on stderr, exit 1, nothing compiled.
rc=0; err=$("$M" 2>&1 >/dev/null) || rc=$?
[ "$rc" = 1 ] || fail "no arguments: exit $rc"
assert_contains "$err" "usage:" "no-argument output"
out=$("$M" --help) || fail "--help failed"
assert_contains "$out" "-o FILE" "--help"

# Unknown options and file types are rejected.
rc=0; err=$("$M" -target=x86_64 -Wall a.c 2>&1) || rc=$?
[ "$rc" = 1 ] || fail "unknown option: exit $rc"
assert_contains "$err" "unknown option -Wall" "unknown option"
rc=0; err=$("$M" -target=x86_64 -o p a.c notes.txt 2>&1) || rc=$?
[ "$rc" = 1 ] || fail "unknown file type: exit $rc"
assert_contains "$err" "unknown file type" "unknown file type"
rc=0; err=$("$M" -target=x86_64 -o p a.c missing.c 2>&1) || rc=$?
[ "$rc" = 1 ] || fail "missing input: exit $rc"
assert_contains "$err" "missing.c" "missing input"

# Two sources linked into a.out, quietly: nothing on stdout (the asm
# used to be echoed there) and the target given after the files.
out=$("$M" a.c b.c -target=x86_64 2>/dev/null) || fail "a.c b.c did not link"
[ -z "$out" ] || fail "the driver printed to stdout: $out"
[ -x a.out ] || fail "a.out was not written"
rc=0; ./a.out || rc=$?
[ "$rc" = 42 ] || fail "a.out exit $rc"
rm -f a.out

# -o, and the original asm-echo-free output: only the executable.
"$M" -target=x86_64 -o prog a.c b.c 2>/dev/null || fail "-o prog failed"
rc=0; ./prog || rc=$?
[ "$rc" = 42 ] || fail "prog exit $rc"
[ "$(ls | sort | tr '\n' ' ')" = "a.c b.c prog tmpdir " ] || fail "files left behind: $(ls)"
[ -z "$(ls "$TMPDIR")" ] || fail "temporaries left in TMPDIR: $(ls "$TMPDIR")"

# -c names NAME.o in the current directory; the objects link.
mkdir -p sub && cp b.c sub/b2.c
"$M" -target=x86_64 -c a.c sub/b2.c 2>/dev/null || fail "-c failed"
[ -f a.o ] && [ -f b2.o ] || fail "-c did not write a.o and b2.o: $(ls)"
file a.o | grep -q 'ELF 64-bit.*relocatable' || fail "a.o is not an object: $(file a.o)"
"$M" -target=x86_64 a.o b2.o -o fromobj 2>/dev/null || fail "linking .o inputs failed"
rc=0; ./fromobj || rc=$?
[ "$rc" = 42 ] || fail "fromobj exit $rc"
"$M" -target=x86_64 -c b.c -o other.o 2>/dev/null || fail "-c -o failed"
[ -f other.o ] && [ ! -f b.o ] || fail "-c -o wrote the wrong file"
rc=0; err=$("$M" -target=x86_64 -c a.c b.c -o x.o 2>&1) || rc=$?
[ "$rc" = 1 ] || fail "-c -o with two sources: exit $rc"
assert_contains "$err" "cannot specify -o" "-c -o with two sources"

# -S writes the asm and stops: no object, no executable.
"$M" -target=x86_64 -S a.c 2>/dev/null || fail "-S failed"
grep -q 'main:' a.s || fail "a.s has no main label"
"$M" -target=aarch64 -S b.c -o b64.s 2>/dev/null || fail "-S -o failed"
grep -q 'helper:' b64.s || fail "b64.s has no helper"
[ ! -e b64.s.o ] || fail "-S assembled"

# -E: preprocessed C, on stdout or in -o FILE.
cat > m.c <<'EOF'
#define TWICE(x) ((x) * 2)
int f(){ return TWICE(21); }
EOF
out=$("$M" -E m.c) || fail "-E failed"
assert_contains "$out" "int f(){ return ((21) * 2); }" "-E output"
case "$out" in *define*) fail "-E kept the #define: $out";; esac
"$M" -E m.c -o m.i || fail "-E -o failed"
assert_eq "$out" "$(cat m.i)" "-E -o contents"
# The preprocessed text compiles to the same program.
cat > m2.c <<'EOF'
int f();
int main(){ return f(); }
EOF
cp m.i m_pre.c
"$M" -target=x86_64 m_pre.c m2.c -o mp 2>/dev/null || fail "the -E output did not compile"
rc=0; ./mp || rc=$?
[ "$rc" = 42 ] || fail "mp exit $rc"

# - reads the source from standard input.
echo 'int main(){ return 7; }' | "$M" -target=x86_64 - -o fromstdin 2>/dev/null || fail "stdin compile failed"
rc=0; ./fromstdin || rc=$?
[ "$rc" = 7 ] || fail "fromstdin exit $rc"
out=$(echo 'int x = 3 + VALUE;' | "$M" -E -DVALUE=4 -)
assert_eq "int x = 3 + 4;" "$out" "-E of stdin"

# A failed compile leaves no output and exits non-zero.
echo 'int main(){ return }' > bad.c
rc=0; "$M" -target=x86_64 bad.c -o badprog >/dev/null 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "a bad source compiled"
[ ! -e badprog ] || fail "a failed compile left badprog"
[ -z "$(ls "$TMPDIR")" ] || fail "temporaries left after a failure: $(ls "$TMPDIR")"

# The output is never one of the inputs.
rc=0; "$M" -target=x86_64 -S a.c -o a.c >/dev/null 2>&1 || rc=$?
[ "$rc" = 1 ] || fail "-o onto the input: exit $rc"
grep -q 'helper(40)' a.c || fail "the input was overwritten"

# -v prints the assembler and linker commands.
err=$("$M" -v -target=x86_64 a.c b.c -o vprog 2>&1 >/dev/null) || fail "-v build failed"
assert_contains "$err" "nasm -f elf64" "-v output"

# The original form still means `in out [object]`; a third argument
# other than object is an error instead of meaning "link".
rc=0; err=$("$M" -target=x86_64 a.c out objekt 2>&1) || rc=$?
[ "$rc" = 1 ] || fail "bad legacy mode: exit $rc"
assert_contains "$err" "unknown mode objekt" "bad legacy mode"
pass
