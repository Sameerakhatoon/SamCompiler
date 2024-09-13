#!/usr/bin/env bash
# Ch149: a top-level `struct foo { int a; int b; } v;` emits the
# variable `v` into .data with `times N db 0` of the struct's size.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch149_input.XXXXXX)
printf 'struct foo { int a; int b; } v;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch149_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch149_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch149_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch149 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "section .data"      ".data section"
assert_contains "$got" "v: dq 0"            "struct variable v reserved 8 bytes (dq)"
pass
