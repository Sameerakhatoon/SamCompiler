#!/usr/bin/env bash
# Ch108: codegen now opens a matched entry / exit pair at the end of
# every compile (placeholder until real loops emit them). Output must
# contain `.entry_point_N:`, `jmp .exit_point_M`, `jmp .entry_point_N`,
# and `.exit_point_M:` in order.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch108_input.XXXXXX)
printf 'int x;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch108_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch108_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch108_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch108 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" ".entry_point_" "entry point label emitted"
assert_contains "$got" ".exit_point_"  "exit point label emitted"
assert_contains "$got" "jmp .entry_point_" "goto entry uses jmp"
assert_contains "$got" "jmp .exit_point_"  "goto exit  uses jmp"
pass
