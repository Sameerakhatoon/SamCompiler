#!/usr/bin/env bash
# Ch105: codegen() now emits .data, .text, .rodata section headers in
# order, while iterating the AST root vector.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch105_input.XXXXXX)
printf 'int x;' > "$scratch"
outfile=$(mktemp /tmp/sam_ch105_out.XXXXXX)

probe=$(mktemp /tmp/sam_ch105_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch105_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch" "$outfile"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    int res = compile_file("${scratch}", "${outfile}", 0);
    printf("res=%d\n", res);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch105 probe failed to compile"
"$bin" >/dev/null
content="$(cat "$outfile")"
assert_contains "$content" "section .data"   ".data section header emitted"
assert_contains "$content" "section .text"   ".text section header emitted"
assert_contains "$content" "section .rodata" ".rodata section header emitted"

# Order: .data before .text before .rodata.
data_at=$(printf '%s\n' "$content" | grep -n 'section .data'  | head -1 | cut -d: -f1)
text_at=$(printf '%s\n' "$content" | grep -n 'section .text'  | head -1 | cut -d: -f1)
rod_at=$(printf  '%s\n' "$content" | grep -n 'section .rodata'| head -1 | cut -d: -f1)
[ "$data_at" -lt "$text_at" ] || fail ".data must precede .text"
[ "$text_at" -lt "$rod_at"  ] || fail ".text must precede .rodata"
pass
