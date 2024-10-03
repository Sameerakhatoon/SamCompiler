#!/usr/bin/env bash
# Ch159: do/while emits the body before the cond and uses `jne` to
# loop back when the cond is non-zero.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch159_input.XXXXXX)
printf 'int main() { int x = 0; do { x += 1; } while(x < 5); return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch159_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch159_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch159_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch159 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" ".do_while_start_"   "do-while start label emitted"
assert_contains "$got" "jne .do_while_start_" "loop back via jne when cond is non-zero"
pass
