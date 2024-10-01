#!/usr/bin/env bash
# Ch4: if test.c is missing, compile_process_create returns NULL and main
# prints "Compile failed".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
# Run from an empty temporary directory, so there is no ./test.c and
# the checkout is never touched (it used to rename ./test.c).
tmp=$(mktemp -d /tmp/sam_missing_input.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# ch143: main now exits non-zero on compile failure; wrap with `|| true`.
# With no arguments the driver prints its usage now instead of
# compiling ./test.c, so name the input.
out="$(cd "$tmp" && "$REPO_ROOT/main" "$tmp/missing.c" "$tmp/out" 2>&1 || true)"
assert_contains "$out" "Compile failed" "main output with a missing input"
[ ! -e "$tmp/out" ] || fail "a missing input left an output file"
pass
