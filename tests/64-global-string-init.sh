#!/usr/bin/env bash
# Ch112: a global initialized to a string literal registers the
# string in .rodata and stores its label in the variable's slot.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch112_input.XXXXXX)
printf 'char* msg = "hi";' > "$scratch"
outfile=$(mktemp /tmp/sam_ch112_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch112_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch112_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch112 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "msg: dd str_"       "msg holds the string label as its value"
assert_contains "$got" "'h', 'i', 0"        "string literal expanded char-by-char"
pass
