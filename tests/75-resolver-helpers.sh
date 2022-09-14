#!/usr/bin/env bash
# Ch127: is_access_operator / is_access_node / is_array_node /
# is_parentheses_node classifiers behave as documented on synthetic
# expression nodes.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch127_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch127_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
int main(void){
    struct node dot   = { .type = NODE_TYPE_EXPRESSION }; dot.exp.op   = ".";
    struct node arrow = { .type = NODE_TYPE_EXPRESSION }; arrow.exp.op = "->";
    struct node arr   = { .type = NODE_TYPE_EXPRESSION }; arr.exp.op   = "[]";
    struct node par   = { .type = NODE_TYPE_EXPRESSION }; par.exp.op   = "()";
    struct node add   = { .type = NODE_TYPE_EXPRESSION }; add.exp.op   = "+";
    printf("dot_acc=%d arrow_acc=%d add_acc=%d\n",
        is_access_node(&dot), is_access_node(&arrow), is_access_node(&add));
    printf("arr_array=%d par_par=%d arrow_acc_arrow=%d arrow_acc_dot=%d\n",
        is_array_node(&arr), is_parentheses_node(&par),
        is_access_node_with_op(&arrow, "->"),
        is_access_node_with_op(&arrow, "."));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch127 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "dot_acc=1 arrow_acc=1 add_acc=0" "access predicate"
assert_contains "$got" "arr_array=1 par_par=1 arrow_acc_arrow=1 arrow_acc_dot=0" "array/parens/access_with_op"
pass
