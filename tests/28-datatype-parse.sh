#!/usr/bin/env bash
# Ch33: feeding `int` to the parser doesn't crash; the datatype
# machinery wakes up and consumes the keyword cleanly without
# producing any node yet (ch34+ adds the variable / function emit).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch33_input.XXXXXX)
printf 'int x;' > "$scratch"

probe=$(mktemp /tmp/sam_ch33_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch33_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    int r = compile_file("${scratch}", "/tmp/sam_ch33_out", 0);
    printf("res=%d\n", r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch33 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "res=0" "compile_file on 'int' returns OK"
pass
