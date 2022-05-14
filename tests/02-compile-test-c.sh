#!/usr/bin/env bash
# Ch4: running ./main on the (empty-ish) test.c opens it, opens ./test
# for write, and reports "everything compiled fine".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
out="$(./main)"
assert_contains "$out" "everything compiled fine" "main output"
[ -f ./test ] || fail "./test output file was not created"
pass
