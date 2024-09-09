#!/usr/bin/env bash
# Run every numbered test in this directory (or the test scripts given
# as arguments) and report pass/fail/skip totals. A test whose toolchain is missing fails, unless
# ALLOW_MISSING_TOOLCHAIN=1 is set; then it exits 77 (SKIP_STATUS in
# lib.sh) and is counted as skipped, never as passed.
set -uo pipefail

cd "$(dirname "$0")/.."

passed=0
failed=0
skipped=0
fails=()
skips=()

if [ $# -gt 0 ]; then
    tests=("$@")
else
    tests=(tests/[0-9]*.sh)
fi

for t in "${tests[@]}"; do
    bash "$t"
    rc=$?
    if [ "$rc" = 0 ]; then
        passed=$((passed + 1))
    elif [ "$rc" = 77 ]; then
        skipped=$((skipped + 1))
        skips+=("$(basename "${t%.sh}")")
    else
        failed=$((failed + 1))
        fails+=("$(basename "${t%.sh}")")
    fi
done

echo
echo "passed: $passed   failed: $failed   skipped: $skipped"
if [ "$skipped" -gt 0 ]; then
    echo "skipped tests (missing toolchain):"
    for s in "${skips[@]}"; do echo "  $s"; done
fi
if [ "$failed" -gt 0 ]; then
    echo "failed tests:"
    for f in "${fails[@]}"; do echo "  $f"; done
    exit 1
fi
