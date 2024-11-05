#!/usr/bin/env bash
# Ch181: `int a, b;` declares two locals at distinct stack slots; an
# assignment to each should land at [ebp-4] and [ebp-8].
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch181_input.XXXXXX)
printf 'int main() { int a, b; a = 50; b = 20; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch181_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch181_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch181_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch181 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "mov dword [ebp-4], eax" "first local lives at [ebp-4]"
assert_contains "$got" "mov dword [ebp-8], eax" "second local lives at [ebp-8]"
pass
