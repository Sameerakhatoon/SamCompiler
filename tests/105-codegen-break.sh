#!/usr/bin/env bash
# Ch161: `break;` emits `jmp .exit_point_<id>` to the innermost
# loop's exit label.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch161_input.XXXXXX)
printf 'int main() { int x; for(x = 0; x < 50; x = x + 1) { break; } return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch161_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch161_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch161_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch161 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "jmp .exit_point_" "break jumps to the innermost exit point"
pass
