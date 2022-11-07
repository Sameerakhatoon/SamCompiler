#!/usr/bin/env bash
# Ch172: `cond ? T : F` emits cmp+je to a false label, the true
# branch, jmp to end, the false label, the false branch, then end.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch172_input.XXXXXX)
printf 'int main() { int x; x = 50 ? 10 : 87; return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch172_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch172_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch172_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch172 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" ".tenary_true_"  "true branch label"
assert_contains "$got" ".tenary_false_" "false branch label"
assert_contains "$got" "je .tenary_false_" "cond jumps to false on zero"
assert_contains "$got" "jmp .tenary_end_"  "true branch jumps to end"
pass
