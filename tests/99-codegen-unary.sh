#!/usr/bin/env bash
# Ch154: normal unaries `-` and `~` emit `neg eax` / `not eax`.
# `*p` indirection emits a chain of `mov ebx, [ebx]`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch154_input.XXXXXX)
printf 'int main() { int a = -5; int b = ~5; int* p; int e = *p; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch154_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch154_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch154_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch154 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "neg eax"        "- unary emits neg"
assert_contains "$got" "not eax"        "~ unary emits not"
assert_contains "$got" "mov ebx, [ebx]" "* unary emits a dereference"
pass
