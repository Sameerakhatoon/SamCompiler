#!/usr/bin/env bash
# Ch170: a string literal in expression position gets registered in
# .rodata and its label moved into eax, then pushed.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch170_input.XXXXXX)
printf 'int printf(const char* s); int main() { printf("hello world\\n"); }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch170_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch170_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch170_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch170 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "'h', 'e', 'l', 'l', 'o'" "string literal expanded in .rodata"
assert_contains "$got" "mov eax, str_"            "string label moved into eax"
assert_contains "$got" "push eax"                 "pushed as argument"
pass
