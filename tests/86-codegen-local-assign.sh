#!/usr/bin/env bash
# Ch138: a local int variable initialized to a literal emits a push
# of the literal followed by a pop into eax and a mov into the
# variable's [ebp-N] slot.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch138_input.XXXXXX)
printf 'int main() { int a = 50; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch138_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch138_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch138_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch138 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "push dword 50"            "literal pushed as the rhs"
assert_contains "$got" "pop eax"                  "popped into eax before store"
assert_contains "$got" "mov dword [ebp-4], eax"   "stored at [ebp-4] (first local)"
pass
