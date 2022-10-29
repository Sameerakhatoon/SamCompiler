#!/usr/bin/env bash
# Ch162: `continue;` emits a jmp to the innermost entry point.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch162_input.XXXXXX)
printf 'int main() { int x; for(x = 0; x < 50; x = x + 1) { continue; } return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch162_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch162_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch162_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch162 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "jmp .entry_point_" "continue jumps to the innermost entry point"
pass
