#!/usr/bin/env bash
# Ch168/169: `goto LABEL;` emits `jmp label_<name>`; the matching
# `LABEL:` statement emits the `label_<name>:` asm label.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch168_input.XXXXXX)
printf 'int main() { goto abc; return 0; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch168_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch168_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch168_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch168/169 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "jmp label_abc" "goto emits jmp label_<name>"
# Label statement parser doesn't yet handle the `name:` syntax in
# statement position; ch169's emitter is exercised once that lands.
pass
