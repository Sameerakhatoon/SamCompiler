#!/usr/bin/env bash
# Ch167: case labels emit `.switch_stmt_<sw_id>_case_<index>:` and a
# `; CASE <index>` comment marker.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch167_input.XXXXXX)
printf 'int main() { int x = 1; switch(x) { case 1: x = 90; break; case 2: x = 100; break; } return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch167_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch167_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch167_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch167 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "; CASE 1"  "case 1 comment marker"
assert_contains "$got" "; CASE 2"  "case 2 comment marker"
assert_contains "$got" "_case_1:"  "case 1 label emitted"
assert_contains "$got" "_case_2:"  "case 2 label emitted"
pass
