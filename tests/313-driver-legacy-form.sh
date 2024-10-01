#!/usr/bin/env bash
# the original `main [-target=X] in out
# [object]` keeps its exact behaviour, which the chapter tests,
# tools/gcc_differential.sh and SamOS's tools rely on: the asm at out,
# the object at out.o, the executable over out, the status lines on
# stdout. Only the asm echo to stdout is gone.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_legacy.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
M="$REPO_ROOT/main"
echo 'int main(){ return 5; }' > "$tmp/p.c"

# in out object: NASM source at out, ELF object at out.o.
out=$("$M" -target=x86_64 "$tmp/p.c" "$tmp/p.s" object) || fail "object mode failed"
assert_contains "$out" "everything compiled fine" "object mode stdout"
assert_contains "$out" "nasm -f elf64" "object mode stdout"
case "$out" in *"mov "*|*"push "*) fail "the asm was echoed to stdout: $out";; esac
grep -q 'main:' "$tmp/p.s" || fail "p.s is not the asm"
file "$tmp/p.s.o" | grep -q 'relocatable' || fail "p.s.o is not an object"

# in out: the executable over out, out.o left as before.
"$M" -target=x86_64 "$tmp/p.c" "$tmp/p" >/dev/null 2>&1 || fail "link mode failed"
rc=0; "$tmp/p" || rc=$?
assert_eq 5 "$rc" "linked program exit status"
[ -f "$tmp/p.o" ] || fail "link mode no longer leaves p.o"

# Options after the file names now count (-target= used to be ignored there).
"$M" "$tmp/p.c" "$tmp/q.s" object -target=aarch64 >/dev/null 2>&1 || fail "trailing -target failed"
grep -q 'stp\|ret' "$tmp/q.s" || fail "-target=aarch64 after the files was ignored"

# SamOS's probe: a samos target without a sysroot names the sysroot,
# not an unknown target, and -I before the files is accepted.
out=$(SAMOS_SYSROOT= "$M" -target=samos-x86_64 -I"$tmp" /dev/null "$tmp/probe" 2>&1) && fail "probe succeeded"
assert_contains "$out" "needs -sysroot=DIR" "samos probe"
case "$out" in *"Unknown target"*) fail "probe: $out";; esac

# A second file that is a source is not an output: `a.c b.c` links both.
echo 'int g(){ return 1; }' > "$tmp/g.c"
echo 'int g(); int main(){ return g() + 1; }' > "$tmp/m.c"
(cd "$tmp" && "$M" -target=x86_64 m.c g.c >/dev/null 2>&1) || fail "m.c g.c failed"
grep -q 'g()' "$tmp/g.c" || fail "g.c was overwritten"
rc=0; "$tmp/a.out" || rc=$?
assert_eq 2 "$rc" "a.out exit status"
pass
