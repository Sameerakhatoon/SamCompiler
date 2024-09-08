#!/usr/bin/env bash
# Ch145: an identifier as an expression now emits a direct value load
# (`push dword [ebp-N]`) instead of address arithmetic. Tested with
# `int b = 50; int e = 20; b = e + 10;` - the `e` reference pushes
# the value at [ebp-8] directly.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch145_input.XXXXXX)
printf 'int main() { int b = 50; int e = 20; b = e + 10; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch145_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch145_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch145_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch145 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "push dword [ebp-8]" "identifier e loads via push dword [ebp-8]"
assert_contains "$got" "push dword 10"       "literal 10 pushed for the rhs of +"
assert_contains "$got" "add eax, ecx"        "arithmetic add lands"
assert_contains "$got" "mov dword [ebp-4], eax" "store result into b"
pass
