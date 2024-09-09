#!/usr/bin/env bash
# samples/run_all.sh with ARCH=. Every sample must return its
# expected exit code on every target whose toolchain is installed
# (x86 and x86_64 always run; aarch64 and riscv64 need the cross
# gcc and qemu-user, and a missing one fails the test unless
# ALLOW_MISSING_TOOLCHAIN=1).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

targets="x86 x86_64"
have_tools aarch64-linux-gnu-gcc qemu-aarch64 && targets="$targets aarch64"
have_tools riscv64-linux-gnu-gcc qemu-riscv64 && targets="$targets riscv64"

for t in $targets; do
    out=$(bash samples/run_all.sh ARCH="$t" 2>&1) || fail "samples failed on $t: $(echo "$out" | grep -v ' OK$' | head -5)"
    assert_contains "$out" "passed: 14   failed: 0" "$t samples"
done

out=$(bash samples/run_all.sh ARCH=sparc 2>&1 && echo "rc=0" || echo "rc=$?")
assert_contains "$out" "unknown ARCH=sparc" "bad ARCH rejected"
assert_contains "$out" "rc=2" "bad ARCH exit code"
pass
