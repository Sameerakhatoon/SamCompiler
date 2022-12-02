#!/usr/bin/env bash
# Ch148: a function call statement emits arg pushes in reverse, a
# call ecx, then add esp to drop the args. Tested with a forward-
# declared external function called with one arg.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch148_input.XXXXXX)
printf 'int puts(int x); int main() { puts(42); }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch148_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch148_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch148_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch148 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "extern puts"     "external symbol declared"
assert_contains "$got" "push dword 42"   "argument pushed"
# ch187 changed the indirect call to go through a per-call .data
# slot instead of ecx (so arguments that are themselves calls can't
# clobber the target).
assert_contains "$got" "call [function_call_" "indirect call via the per-call .data slot"
assert_contains "$got" "add esp,"        "stack reclaimed after the call"
pass
