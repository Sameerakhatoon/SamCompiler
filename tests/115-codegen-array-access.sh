#!/usr/bin/env bash
# Ch174: `int a[4]; int x = a[2];` emits an array-bracket access:
# resolve a as the base, evaluate the index, scale by element size,
# add to base, then a final load (driven by codegen_resolve_node_for_value).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch174_input.XXXXXX)
printf 'int main() { int a[4]; int i; int x = a[i]; return x; }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch174_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch174_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch174_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch174 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
# JUST_USE_OFFSET path: constant index 2 was already folded into the
# entity offset, so we expect either an `add ebx, <offset>` (constant
# fold) or an `imul eax, <element_size>` + `add ebx, eax` (runtime).
case "$got" in
  *"add ebx, "*) ;;
  *) fail "expected an 'add ebx, ...' after array index resolution: $got" ;;
esac
pass
