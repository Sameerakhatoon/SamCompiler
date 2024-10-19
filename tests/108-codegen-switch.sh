#!/usr/bin/env bash
# Ch166: `switch (x) { case N: ... break; }` emits the jump-table
# (cmp+je per case) followed by the body.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch166_input.XXXXXX)
printf 'int main() { int x = 3; switch(x) { case 3: x = 90; break; case 1: x = 20; break; } return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch166_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch166_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch166_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch166 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" ".switch_stmt_"   "switch start label emitted"
assert_contains "$got" "cmp eax, 3"      "case 3 compared"
assert_contains "$got" "cmp eax, 1"      "case 1 compared"
assert_contains "$got" "je .switch_stmt_" "case dispatch via je"
pass
