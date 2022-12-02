#!/usr/bin/env bash
# Ch191: expressionable type declarations land in compiler.h. Just
# a smoke test that all the enums + struct + callback typedefs are
# usable from a probe.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch191_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch191_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
int main(void){
    struct expressionable e = {0};
    e.flags = EXPRESSIONABLE_FLAG_IS_PREPROCESSOR_EXPRESSION;
    int t = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
    int single = EXPRESSIONABLE_IS_SINGLE;
    int parens = EXPRESSIONABLE_IS_PARENTHESES;
    printf("flag=%d t=%d s=%d p=%d\n", e.flags, t, single, parens);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch191 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "flag=1 t=0 s=0 p=1" "expressionable types compile and have expected values"
pass
