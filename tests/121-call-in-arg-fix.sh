#!/usr/bin/env bash
# Ch187: a function call whose argument is itself a function call
# doesn't clobber the outer callee anymore. Two distinct per-call
# .data slots should appear in the asm.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch187_input.XXXXXX)
printf 'int printf(const char* s, ...); int special(int x); int main() { printf("%%i\\n", special(10)); }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch187_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch187_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch187_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch187 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
# Two distinct per-call slots in .data.
count=$(grep -c "^function_call_[0-9]*: dd 0$" "$outfile" || true)
[ "$count" -ge 2 ] || fail "expected >=2 per-call slots; got $count"
assert_contains "$got" "section .data" "data section flushed at the end too"
pass
