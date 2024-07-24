#!/usr/bin/env bash
# Ch110: codegen registers a few literal strings at the end of every
# compile and emits them in .rodata as `str_N: db 'H', 'e', ..., 0`
# lines. Duplicate registrations collapse to one entry; '\n' is
# emitted as decimal 10.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch110_input.XXXXXX)
printf 'int x;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch110_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch110_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch110_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch110 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "section .rodata"           ".rodata header present"
assert_contains "$got" "'H', 'e', 'l', 'l', 'o'"   "Hello literal expanded char-by-char"
assert_contains "$got" "'A', 'b', 'c', 10, 0"      "Abc escape: \\n -> 10, terminator 0"
# Dedup: "Hello world!!" registered 3 times should appear only once.
count=$(grep -c "'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o'" "$outfile" || true)
[ "$count" -eq 1 ] || fail "expected exactly 1 Hello entry; got $count"
pass
