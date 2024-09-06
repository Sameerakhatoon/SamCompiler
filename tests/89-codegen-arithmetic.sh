#!/usr/bin/env bash
# Ch142: a local int initialized to a binary arithmetic expression
# emits push left, push right, pop ecx, pop eax, then the op.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch142_input.XXXXXX)
printf 'int main() { int a = 3 + 4; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch142_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch142_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch142_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch142 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "push dword 3"   "left operand pushed"
assert_contains "$got" "push dword 4"   "right operand pushed"
assert_contains "$got" "pop ecx"        "right popped into ecx"
assert_contains "$got" "pop eax"        "left popped into eax"
assert_contains "$got" "add eax, ecx"   "addition performed"
assert_contains "$got" "mov dword [ebp-4], eax" "result stored at the local"
pass
