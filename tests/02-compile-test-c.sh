#!/usr/bin/env bash
# Ch4: running ./main on the (empty-ish) test.c opens it, opens ./test
# for write, and reports "everything compiled fine".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
# ch143 made ./main shell out to nasm+gcc by default. Pass "object"
# so it stops after NASM - test.c has no `main()` so we'd otherwise
# fail at the gcc link step.
out="$(./main ./test.c ./test object 2>&1 || true)"
assert_contains "$out" "everything compiled fine" "main output"
[ -f ./test ] || fail "./test output file was not created"
pass
