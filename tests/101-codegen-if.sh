#!/usr/bin/env bash
# Ch157: `if (cond) { ... }` emits cond -> cmp eax, 0 -> je .if_N ->
# body -> jmp .if_end_M -> .if_N:. else / else-if chain into the same
# end label.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch157_input.XXXXXX)
printf 'int main() { int x = 5; if (x) { return 1; } else { return 0; } }' > "$scratch"
outfile=$(mktemp /tmp/sam_ch157_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch157_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch157_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){ return compile_file("${scratch}", "${outfile}", 0); }
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch157 probe failed to compile"
"$bin" >/dev/null
got="$(cat "$outfile")"
assert_contains "$got" "cmp eax, 0"  "condition compared to 0"
assert_contains "$got" "je .if_"     "false branch jumps to .if_N"
assert_contains "$got" "jmp .if_end_" "true branch jumps to end label"
assert_contains "$got" ".if_end_"     "end-of-if label emitted"
pass
