#!/usr/bin/env bash
# Ch34: parser can chew through `int**` (datatype keyword + 2 pointer
# operators) without crashing. parse_datatype_type collects the
# pointer depth via parser_get_pointer_depth.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch34_input.XXXXXX)
printf 'int** x' > "$scratch"

probe=$(mktemp /tmp/sam_ch34_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch34_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    int r = compile_file("${scratch}", "/tmp/sam_ch34_out", 0);
    printf("res=%d\n", r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch34 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "res=0" "compile_file on 'int**' returns OK"
pass
