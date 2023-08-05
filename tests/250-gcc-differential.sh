#!/usr/bin/env bash
# Every program in
# tests/diff is compiled by gcc and by SamCompiler for each target and both
# exit statuses must match. Each fixed miscompile adds its program here.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

out=$(bash tools/gcc_differential.sh tests/diff 2>&1) || fail "differences against gcc:
$out"
assert_contains "$out" "mismatches: 0" "differential summary"

pass
