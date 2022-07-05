#!/usr/bin/env bash
# Ch36: keywords now route through parse_keyword_for_global at top level.
# Input `unsigned char` parses cleanly (modifier + datatype) without crashing.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch36_input.XXXXXX)
printf 'unsigned char x;' > "$scratch"

probe=$(mktemp /tmp/sam_ch36_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch36_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    int r = compile_file("${scratch}", "/tmp/sam_ch36_out", 0);
    printf("res=%d\n", r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch36 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "res=0" "unsigned char parses cleanly"
pass
