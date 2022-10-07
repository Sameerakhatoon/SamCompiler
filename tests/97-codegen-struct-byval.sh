#!/usr/bin/env bash
# Ch150: struct-by-value assignment + access emits the "STRUCTURE
# PUSH" chunked copy. Tested with `struct foo { int a; int b; } x, y;
# x = y;` - reading y pushes both dwords; the assignment pops them
# and stores into x's slots.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch150_input.XXXXXX)
printf 'struct foo { int a; int b; }; struct foo x; struct foo y; int main() { x = y; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch150_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch150_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch150_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch150 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "; STRUCTURE PUSH"    "structure-push prologue comment"
assert_contains "$got" "; END STRUCTURE PUSH" "structure-push epilogue comment"
# Each chunk pop + store into x at its declared address.
assert_contains "$got" "lea ebx, [y"          "load address of y"
assert_contains "$got" "pop eax"              "pop chunk into eax"
assert_contains "$got" "mov [x"               "store chunk into x"
pass
