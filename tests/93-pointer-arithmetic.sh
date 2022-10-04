#!/usr/bin/env bash
# Ch146: pointer arithmetic scales the integer operand by the pointed-
# to element size before the add. `int* p; p + 1` should emit
# `imul ecx, 4` so the offset advances one int (4 bytes).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch146_input.XXXXXX)
printf 'int main() { int* p; int x = p + 1; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch146_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch146_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch146_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch146 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
# pointer is on the left -> ecx (the int RHS) gets scaled by 4.
assert_contains "$got" "imul ecx, 4" "int operand scaled by sizeof(int) for pointer arithmetic"
assert_contains "$got" "add eax, ecx" "addition performed after the scale"
pass
