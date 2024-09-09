#!/usr/bin/env bash
# tools/compare_gcc.sh writes SamCompiler, gcc -O0 and gcc -O2
# output for a sample side by side and prints instruction counts.
# Checked on x86_64 (always installed) and on the cross targets when
# their gcc is present.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_cmp.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

targets="x86_64"
have_tools i686-linux-gnu-gcc && targets="$targets x86"
have_tools aarch64-linux-gnu-gcc && targets="$targets aarch64"
have_tools riscv64-linux-gnu-gcc && targets="$targets riscv64"

for t in $targets; do
    out=$(OUT="$tmp" bash tools/compare_gcc.sh ARCH="$t" 09_factorial_recursion 2>&1) || fail "compare_gcc failed on $t: $out"
    assert_contains "$out" "09_factorial_recursion" "$t table row"
    for f in sam.s O0.s O2.s side.txt; do
        [ -s "$tmp/$t/09_factorial_recursion.$f" ] || fail "$t: missing $f"
    done
    grep -q "factorial" "$tmp/$t/09_factorial_recursion.O2.s" || fail "$t: gcc -O2 output has no factorial"
done

# sample 07 needs pc_includes ahead of the system headers for gcc
out=$(OUT="$tmp" bash tools/compare_gcc.sh ARCH=x86_64 07_include_user_header 2>&1)
grep -q "could not compile" "$tmp/x86_64/07_include_user_header.O0.s" && fail "gcc fallback for sample 07 did not work"

out=$(bash tools/compare_gcc.sh ARCH=mips 2>&1 && echo "rc=0" || echo "rc=$?")
assert_contains "$out" "rc=2" "bad ARCH rejected"
pass
