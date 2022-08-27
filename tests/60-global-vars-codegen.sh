#!/usr/bin/env bash
# Ch106: global int and char variables emit dd / db lines in .data.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch106_input.XXXXXX)
printf 'int x; int y; char e;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch106_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch106_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch106_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    return compile_file("${scratch}", "${outfile}", 0);
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch106 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "x: dd 0" "int x emits dd 0"
assert_contains "$got" "y: dd 0" "int y emits dd 0"
assert_contains "$got" "e: db 0" "char e emits db 0"
pass
