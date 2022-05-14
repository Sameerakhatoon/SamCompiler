#!/usr/bin/env bash
# Run every numbered test in this directory and report pass/fail totals.
set -uo pipefail

cd "$(dirname "$0")/.."

passed=0
failed=0
fails=()

for t in tests/[0-9]*.sh; do
    if bash "$t"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
        fails+=("$(basename "${t%.sh}")")
    fi
done

echo
echo "passed: $passed   failed: $failed"
if [ "$failed" -gt 0 ]; then
    echo "failed tests:"
    for f in "${fails[@]}"; do echo "  $f"; done
    exit 1
fi
