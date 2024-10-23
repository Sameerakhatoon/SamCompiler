#!/usr/bin/env bash
# Ch171: a parenthesised expression as RHS just walks through to its
# inner expression. `int x; x = (50);` should push the literal 50.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch171_input.XXXXXX)
printf 'int main() { int x; x = (50); }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch171_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch171_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch171_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch171 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "push dword 50"           "literal 50 pushed through parens"
assert_contains "$got" "mov dword [ebp-4], eax"  "stored at the local x"
pass
