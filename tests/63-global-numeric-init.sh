#!/usr/bin/env bash
# Ch111: global int initializer literal value reaches the emitted asm
# (`int x = 42;` -> `x: dd 42`). Zero-init globals stay `x: dd 0`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch111_input.XXXXXX)
printf 'int x = 42; int y = 7; int z;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch111_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch111_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch111_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch111 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "x: dd 42" "int x = 42 emits dd 42"
assert_contains "$got" "y: dd 7"  "int y = 7  emits dd 7"
assert_contains "$got" "z: dd 0"  "int z (no init) still emits dd 0"
pass
