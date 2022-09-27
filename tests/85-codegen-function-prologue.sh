#!/usr/bin/env bash
# Ch137: codegen now emits a function prologue + epilogue for
# function-with-body nodes:
#   global foo
#   foo: push ebp / mov ebp, esp / sub esp, N / ... / add esp, N /
#        pop ebp / ret
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch137_input.XXXXXX)
printf 'int main() { int a; int b; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch137_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch137_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch137_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch137 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "global main"   "function symbol exported"
assert_contains "$got" "main:"         "function entry label"
assert_contains "$got" "push ebp"      "save ebp on entry"
assert_contains "$got" "mov ebp, esp"  "set up frame pointer"
# Two ints = 8, aligned to 16.
assert_contains "$got" "sub esp, 16"   "stack reserved for locals (aligned to 16)"
assert_contains "$got" "add esp, 16"   "stack restored on exit"
assert_contains "$got" "pop ebp"       "restore ebp on exit"
assert_contains "$got" "ret"           "function returns"
pass
