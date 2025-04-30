#!/usr/bin/env bash
# the suite runs the compiler under a memory and a time cap, so
# a runaway compile fails a test instead of taking the machine's memory
# (a compile grew to 9 GB once and the kernel killed every other job
# on the machine). Checks that samcc (tests/lib.sh) passes a compile through,
# fails one that needs more memory than the cap or more time than the
# timeout, and that the gcc differential caps its compiles.
. "$(dirname "$0")/lib.sh"
need_tools prlimit timeout

tmp=$(mktemp -d /tmp/sam_caps.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# A compile within the caps works as ./main does.
samcc -target=x86_64 -S samples/01_arithmetic_control_flow.c -o "$tmp/ok.s" >/dev/null 2>&1 || fail "samcc did not compile a sample"
./main -target=x86_64 -S samples/01_arithmetic_control_flow.c -o "$tmp/plain.s" >/dev/null 2>&1 || fail "./main did not compile a sample"
cmp -s "$tmp/ok.s" "$tmp/plain.s" || fail "samcc gives other output than ./main"

# Stdin and the exit status pass through.
rc=0
echo 'int main(void){ return undeclared; }' | samcc -target=x86_64 -S - -o "$tmp/bad.s" >/dev/null 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "samcc hid a failed compile"

# A compile over the memory cap fails, with a non-zero status, and the
# test goes on.
{
    for i in $(seq 1 3000); do echo "int f$i(int a){ return a + $i; }"; done
    echo 'int main(void){ return 0; }'
} > "$tmp/big.c"
rc=0
SAMCC_MEMORY_CAP=16777216 samcc -target=x86_64 -S "$tmp/big.c" -o "$tmp/big.s" >/dev/null 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "a compile over a 16 MB cap succeeded"

# A compile over the time limit is stopped (status 124).
rc=0
SAMCC_TIMEOUT=0.01 samcc -target=x86_64 -S "$tmp/big.c" -o "$tmp/big.s" >/dev/null 2>&1 || rc=$?
assert_eq 124 "$rc" "status of a compile over the time limit"

# The gcc differential caps every SamCompiler compile.
grep -q 'cap="prlimit --as=4294967296"' tools/gcc_differential.sh || fail "the gcc differential sets no memory cap"
grep -q '\$cap ./main' tools/gcc_differential.sh || fail "the gcc differential runs ./main without the cap"
pass
