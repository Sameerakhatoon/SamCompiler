#!/usr/bin/env bash
# Ch29: expressionable.c exposes the op_precedence table that ch30+ will
# index into. Just sanity that it links and the table is reachable.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch29_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch29_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "compiler.h"
// ch30 moved the type defs into compiler.h, so we just extern the
// table here.
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

int main(void){
    // The "*" operator should sit in some group; find it and print
    // the group index.
    for(int i = 0; i < TOTAL_OPERATOR_GROUPS; i++){
        for(int b = 0; op_precedence[i].operators[b]; b++){
            if(strcmp(op_precedence[i].operators[b], "*") == 0){
                printf("found * at group=%d slot=%d\n", i, b);
            }
            if(strcmp(op_precedence[i].operators[b], "+") == 0){
                printf("found + at group=%d slot=%d\n", i, b);
            }
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch29 probe failed to compile"
got="$("$bin")"
# Per the table: * lives at group 1 (after ++/--/etc at group 0),
# + lives at group 2 (after *,/,%). Lower index = higher precedence.
assert_contains "$got" "found * at group=1"  "* in precedence group 1 (higher precedence)"
assert_contains "$got" "found + at group=2"  "+ in precedence group 2 (lower precedence than *)"
pass
