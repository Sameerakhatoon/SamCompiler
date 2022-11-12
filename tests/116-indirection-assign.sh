#!/usr/bin/env bash
# Ch176: `*p = 50;` parses cleanly and emits a real store. Before the
# fix, the unary operand parser would greedily eat the `=` operator
# as part of the unary descent and miscompile.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch176_input.XXXXXX)
printf 'int main() { int* p; *p = 50; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch176_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch176_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch176_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch176 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "push dword 50"   "rhs literal pushed"
# Indirection writes through the pointer: `mov [edx], <reg>` or similar.
case "$got" in
    *"mov "*"[edx]"*) ;;
    *"mov "*"[ebx]"*) ;;
    *) fail "expected an indirect store after *p = 50: $got" ;;
esac
pass
