#!/usr/bin/env bash
# Ch128: is_argument_operator / is_argument_node / node_valid.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch128_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch128_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
int main(void){
    struct node comma = { .type = NODE_TYPE_EXPRESSION }; comma.exp.op = ",";
    struct node add   = { .type = NODE_TYPE_EXPRESSION }; add.exp.op   = "+";
    struct node num   = { .type = NODE_TYPE_NUMBER };
    struct node blank = { .type = NODE_TYPE_BLANK };
    printf("comma=%d add=%d num_valid=%d blank_valid=%d null_valid=%d\n",
        is_argument_node(&comma), is_argument_node(&add),
        node_valid(&num), node_valid(&blank), node_valid(NULL));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch128 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "comma=1 add=0 num_valid=1 blank_valid=0 null_valid=0" "argument predicates + node_valid"
pass
