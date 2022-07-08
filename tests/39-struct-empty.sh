#!/usr/bin/env bash
# Ch47: `struct abc { };` parses cleanly. The struct body is currently
# walked-and-discarded by the stub; real body parsing is ch48+.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch47_input.XXXXXX)
printf 'struct abc { };' > "$scratch"

probe=$(mktemp /tmp/sam_ch47_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch47_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
int main(void){
    int r = compile_file("${scratch}", "/tmp/sam_ch47_out", 0);
    printf("res=%d\n", r);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch47 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "res=0" "struct abc {}; parses OK"
pass
