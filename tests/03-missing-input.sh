#!/usr/bin/env bash
# Ch4: if test.c is missing, compile_process_create returns NULL and main
# prints "Compile failed".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
mv ./test.c ./test.c.bak
trap 'mv ./test.c.bak ./test.c 2>/dev/null || true' EXIT

# ch143: main now exits non-zero on compile failure; wrap with `|| true`.
out="$(./main 2>&1 || true)"
assert_contains "$out" "Compile failed" "main output"
pass
