#!/usr/bin/env bash
# Ch178: global union variable lands in .data sized to the largest
# member. `union abc { int x; int y; } a;` should emit
# `a: times 4 db 0` (4 bytes, max of two ints).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch178_input.XXXXXX)
printf 'union abc { int x; int y; }; union abc a;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch178_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch178_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch178_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch178 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "a:"  "union variable label emitted"
# 4-byte union -> `a: dd 0`.
assert_contains "$got" "a: dd 0" "union sized to 4 bytes (largest int)"
pass
