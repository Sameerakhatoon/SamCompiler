#!/usr/bin/env bash
# Ch179: global array variable lands in .data sized to total bytes.
# `int xs[4];` should emit `xs:` followed by a reservation of 16
# bytes (either `times 16 db 0` or 4x `dd 0`).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch179_input.XXXXXX)
printf 'int xs[4];' > "$scratch"
outfile=$(mktemp /tmp/sam_ch179_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch179_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch179_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch179 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "xs:"            "array label emitted"
assert_contains "$got" "times 16 db"    "4*int = 16 bytes reserved"
pass
