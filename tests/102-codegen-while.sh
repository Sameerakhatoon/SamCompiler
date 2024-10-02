#!/usr/bin/env bash
# Ch158: while loop emits `.while_start_N:` / cmp + `je
# .while_end_M` / body / `jmp .while_start_N` / `.while_end_M:`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch158_input.XXXXXX)
printf 'int main() { int x = 0; while(x < 50) { x += 1; } return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch158_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch158_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch158_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch158 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" ".while_start_"  "while start label emitted"
assert_contains "$got" "je .while_end_" "condition jumps to end on zero"
assert_contains "$got" "jmp .while_start_" "tail jumps back to the start"
assert_contains "$got" ".while_end_"    "end label emitted"
pass
