#!/usr/bin/env bash
# Ch173: `(char) x` truncates an int to a byte via movsx/movzx.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch173_input.XXXXXX)
printf 'int main() { int x = 50; char c = (char) x; return c; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch173_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch173_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch173_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch173 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
# (char) x narrows via movsx eax, al (since char is signed by default in our codegen).
assert_contains "$got" "eax, al" "cast narrows via eax/al register pair"
pass
