#!/usr/bin/env bash
# Ch151: &x emits an lea + push of the address (not a value load).
# `int b; int* p = &b;` should produce `lea ebx, [ebp-N]` before the
# push that initializes p.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch151_input.XXXXXX)
printf 'int main() { int b; int* p = &b; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch151_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch151_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch151_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch151 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
# Address-of emits lea, not a value load.
assert_contains "$got" "lea ebx, [ebp-4]"  "address of b is loaded via lea"
assert_contains "$got" "push ebx"          "address pushed onto the stack"
# p is the second local: [ebp-8].
assert_contains "$got" "mov dword [ebp-8], eax" "address stored into p"
pass
