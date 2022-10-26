#!/usr/bin/env bash
# Ch160: for loop emits init -> .for_loop<N>: -> cond -> body ->
# loop-expr -> jmp .for_loop<N> -> .for_loop_end<M>:.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch160_input.XXXXXX)
printf 'int main() { int sum = 0; int i; for(i = 0; i < 10; i = i + 1) { sum = sum + i; } return sum; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch160_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch160_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch160_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch160 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" ".for_loop"     "for-loop body label emitted"
assert_contains "$got" "je .for_loop_end" "cond exits loop on zero"
assert_contains "$got" "jmp .for_loop"    "tail jumps back to the start"
assert_contains "$got" ".for_loop_end"    "end label emitted"
pass
